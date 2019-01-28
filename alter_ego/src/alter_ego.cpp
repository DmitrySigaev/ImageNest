/****************************************************************************
 * Copyright (C) 2009-2010 GGA Software Services LLC
 * 
 * This file is part of Imago toolkit.
 * 
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ***************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "imago_c.h"

static qword id = 0;
#define RELEASE    do { if (id > 0) imagoReleaseSessionId(id); } while(0)
#define EXIT(code) do { RELEASE; return code; } while(0)
#define HELP(name) do { printHelp(name); EXIT(0); } while(0)
#define CALL(func) do { if (!(func)) { fprintf(stderr, "%s\n", imagoGetLastError()); EXIT(2); } } while(0)

/* NOTE THE CORRECT API FUNCTION CALL ORDER:

   imagoAllocSessionId()
   imagoSetSessionId(id)
      imagoLoadImageFromFile(file)
      imagoSetFilter(filter)
      imagoFilterImage()
	     imagoSetConfig(config)
		 imagoRecognize()
	  imagoSaveMolToFile(output_name) / imagoSaveMolToBuffer(&buf, &size)
	imagoReleaseSessionId(id)
*/

const int WARNINGS_TRESHOLD = 9;

void printHelp( const char *exe_name )
{
   fprintf(stderr, "Usage: %s [Options] <infile.png>\n\n"
      "Options:\n"
      "    -h                Display this help message\n"
      "    -o <outfile.mol>  If not mentioned molecule will be stored in <infile.mol>\n" 
	  "                          use \"-o -\" for saving the molecule to the standard output\n"
	  "    -f <filter>       Force-select filter type: \"prefilter_binarized\", \"prefilter_basic\", \"...\"\n"
	  "                          by default the best result of possible ones will be selected\n"
	  "    -c <config_file>  Force-select configuration cluster\n"
	  "    -l                Enable logging (slowdowns process, disabled by default)\n"
	  "    -i <value>        Drop images with ink percentage greater that value\n"
      "    -p <directory>    Directory prefix for output file\n",
	  exe_name, exe_name, exe_name);
}

void formPrettyExeName( char *full )
{
   int n = strlen(full), i;

   for (i = n - 1; i >= 0; i--)
      if (full[i] == '\\' || full[i] == '/')
         break;

   if (i != 0)
   {
      i += 1;

      for (int j = 0; j < n - i; j++)
         full[j] = full[i + j];

      full[n - i] = 0;
   }
}

int main( int argc, char **argv )
{
   int bin_level = -1;
   char *in_file_name = 0, *out_file_name = 0, *out_directory_prefix = 0;
   bool no_output = false, standard_output = false;

   formPrettyExeName(argv[0]);

   qword id = imagoAllocSessionId();
   imagoSetSessionId(id);
   
   //Check & parse command-line parameters
   if (argc < 2)
   {
      HELP(argv[0]);
      EXIT(1);
   }

   std::string imagoFilterIdName = "";
   std::string imagoConfigName = "";
   int dropInkPercentage = -1;

   for (int i = 1; i < argc; i++)
   {
      char *str = argv[i];
	  
      if (strcmp(str, "-c") == 0)
      {
         if (++i == argc)
         {
            fprintf(stderr, "expected value after -c\n");
            EXIT(1);
         }
		 imagoConfigName = argv[i];
      }
      else if (strcmp(str, "-f") == 0)
      {
         if (++i == argc)
         {
            fprintf(stderr, "expected value after -f\n");
            EXIT(1);
         }
         imagoFilterIdName = argv[i];
      }
      else if (strcmp(str, "-i") == 0)
      {
         if (++i == argc)
         {
            fprintf(stderr, "expected value after -i\n");
            EXIT(1);
         }
         dropInkPercentage = atoi(argv[i]);
      }
      else if (strcmp(str, "-h") == 0)
      {
         HELP(argv[0]);
      }
      else if (strcmp(str, "-l") == 0)
      {
         CALL( imagoSetLogging(true) );
      }
      else if (strcmp(str, "-o") == 0)
      {
         if (++i == argc)
         {
            fprintf(stderr, "expected file name after -o\n");
            EXIT(1);
         }

         out_file_name = argv[i];

         if (strcmp(out_file_name, "-") == 0)
            standard_output = true;
      }
      else if (strcmp(str, "-p") == 0)
      {
         if (++i == argc)
         {
            fprintf(stderr, "expected directory path after -p\n");
            EXIT(1);
         }

         out_directory_prefix = argv[i];
      }
      else
      {
         in_file_name = argv[i];
         int n = strlen(in_file_name);

         if (in_file_name[n - 4] != '.')
         {
            fprintf(stderr, "%s is unexpected file (no extension detected)\n", in_file_name);
            EXIT(1);
         }
      }
   }

   if (out_file_name == 0)
      no_output = true;

   if (in_file_name == 0)
   {
      fprintf(stderr, "no input file\n");
      EXIT(1);
   }

   std::string configs_list = imagoGetConfigsList();   

   // main process
retry: // allowing some retries while process

   // load / reload image
   CALL( imagoLoadImageFromFile(in_file_name) ); 

   // setup filter
   if (!imagoFilterIdName.empty())
   {
      CALL( imagoSetFilter(imagoFilterIdName.c_str()) );
   }

   // call filter   
   CALL( imagoFilterImage() );

   if (dropInkPercentage > 0)
   {
	   double ink = 0.0;
	   CALL ( imagoGetInkPercentage(&ink) );
	   ink *= 100.0;
	   if (ink > dropInkPercentage)
	   {
		   fprintf(stderr, "Error: image fill percentage (%f) is greater than max specified (%i).\n", ink, dropInkPercentage);
		   EXIT(3);
	   }
   }

   // update config cluster
   CALL( imagoSetConfig(imagoConfigName.c_str()) );
   
   int recognitionWarningsCount = 0;
   // call the recognition method from API
   bool good = imagoRecognize(&recognitionWarningsCount) > 0;

   if (recognitionWarningsCount > 0)
   {
	   fprintf(stderr, "Warnings for filter '%s': %i\n", imagoFilterIdName.c_str(), recognitionWarningsCount);
   }

   if (!good)
   {
	   fprintf(stderr, "Error: %s\n", imagoGetLastError());
   }

   if ((!good || recognitionWarningsCount > WARNINGS_TRESHOLD) && imagoFilterIdName != "prefilter_basic")
   {
	   imagoFilterIdName = "prefilter_basic";
	   goto retry;
   }


   /// store result if got one

   if (good)
   {
      // crop input file name
      int n = strlen(in_file_name), slash_delim = -1;
      for (int i = n - 1; i >= 0; i--)
      {
         if (in_file_name[i] == '\\' || in_file_name[i] == '/')
         {
            slash_delim = i;
            break;
         }
      }
      strcpy(in_file_name, in_file_name + slash_delim + 1);

      //Write result in infile.mol
      if (no_output)
      {
         int n = strlen(in_file_name), k;

         k = n - 4;

         out_file_name = new char[n + 1];
         strcpy(out_file_name, in_file_name);
         strcpy(out_file_name + k + 1, "mol");
      }

      //Save molecule in file 
      if (!standard_output)
      {
         int n1 = strlen(out_file_name), n2;

         if (out_directory_prefix != 0)
            n2 = strlen(out_directory_prefix);
         else
            n2 = 0;

         char *output_name = new char[n1 + n2 + 2];

         if (out_directory_prefix != 0)
         {
            strcpy(output_name, out_directory_prefix);
            strcat(output_name, "/");
            strcat(output_name, out_file_name);
         }
         else
            strcpy(output_name, out_file_name);

         CALL(imagoSaveMolToFile(output_name));

         if (no_output)
            delete out_file_name;

         delete output_name;
      }
      else // output to stdout
      {
         int size = 0;
         char *buf = NULL;

         CALL(imagoSaveMolToBuffer(&buf, &size));

         puts(buf);
      }

      EXIT(0);
   }   
   else
   {
      fprintf(stderr, "No result achieved.\n");

      EXIT(2);
   }
}