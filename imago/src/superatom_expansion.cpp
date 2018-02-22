#include "boost/foreach.hpp"

#include "superatom_expansion.h"

#include "indigo.h"

#include "molecule.h"
#include "output.h"
#include "molfile_saver.h"

namespace imago
{  
   std::string expandSuperatoms( const Molecule &molecule )
   {
      using namespace std;

      static bool init = false;
      static map<string, string> abbr_map;
      typedef pair<int, string> pair_i_s;

      if (!init)
      {
         abbr_map["TfO"] = "FC(F)(F)S(=O)(=O)O*";
         abbr_map["COOEt"] = "CCOC(*)=O";
         abbr_map["iPrS"] = "CC(C)S*";
         abbr_map["iPrO"] = "CC(C)O*";
         abbr_map["SiPr"] = "CC(C)S*";
         abbr_map["EtOOC"] = "CCOC(*)=O";
         abbr_map["HOOC"] = "OC(*)=O";
         abbr_map["HO2C"] = "OC(*)=O";
         abbr_map["Ph"] = "*C1=CC=CC=C1";
         abbr_map["CF3"] = "FC(F)(F)*";
         abbr_map["NO2"] = "*N(=O)=O"; // This should not be [O-][N+](*)=O
         abbr_map["OiPr"] = "CC(C)O*";
         abbr_map["F3C"] = "FC(F)(F)*";
         abbr_map["ONO"] = "*ON=O";
         abbr_map["NC"] = "*C#N";
         abbr_map["OCN"] = "*N=C=O";
         abbr_map["OCH3"] = "CO[*]";
         abbr_map["HO2S"] = "OS(*)=O";
         abbr_map["H2O3P"] = "OP(O)(*)=O";
         abbr_map["SO2H"] = "OS(*)=O";
         abbr_map["ONO2"] = "*ON(=O)=O";
         abbr_map["CN"] = "*C#N";
         abbr_map["TMSO"] = "C[Si](C)(C)O*";
         abbr_map["Si-Pr"] = "CC(C)S*";
         abbr_map["OTf"] = "FC(F)(F)S(=O)(=O)O*";
         abbr_map["O2N"] = "[O-][N+](*)=O";
         abbr_map["PO3H2"] = "OP(O)(*)=O";
         abbr_map["i-PrS"] = "CC(C)S*";
         abbr_map["COOH"] = "OC(*)=O";
         abbr_map["CO2H"] = "OC(*)=O";
         abbr_map["OTMS"] = "C[Si](C)(C)O*";
         abbr_map["COOMe"] = "COC(*)=O";
         abbr_map["O2NO"] = "*ON(=O)=O";
         abbr_map["OBz"] = "*OC(=O)C1=CC=CC=C1";
         abbr_map["HO3S"] = "OS(*)(=O)=O";
         abbr_map["NCO"] = "*N=C=O";
         abbr_map["BzO"] = "*OC(=O)C1=CC=CC=C1";
         abbr_map["MeOOC"] = "COC(*)=O";
         abbr_map["SO3H"] = "OS(*)(=O)=O\0";		
         init = true;
      }

      string molString;
      ArrayOutput so(molString);
      MolfileSaver ma(so);
      ma.saveMolecule(molecule);
      
      indigoSetOption("ignore-stereochemistry-errors", "true");

      int mol = indigoLoadMoleculeFromString(molString.c_str());
      //printf("***$%d\n", mol);

      if (mol == -1)
      {
         fprintf(stderr, "%s\n", indigoGetLastError());
         return molString;
      }

      int item, iter = indigoIterateAtoms(mol);
      //printf("***%d\n", iter);
      
      list<pair<int, string> > to_replace;
      vector<int> to_layout;

      while (item = indigoNext(iter))
      {
         //printf("$$$%d\n", item);
         if (item == -1)
            throw LogicException("%s", indigoGetLastError());
   
        string symbol = indigoSymbol(item);
        map<string, string>::const_iterator it;
        if ((it = abbr_map.find(symbol)) != abbr_map.end())
        {
           to_replace.push_back(make_pair(item, it->second));
        }
        else
           indigoFree(item);
      }
      indigoFree(iter);

      if (to_replace.size() == 0)
         return molString;

      BOOST_FOREACH(pair_i_s p, to_replace)
      {
         //printf("\t%s\n", p.second.c_str());
         const char *name = indigoSymbol(p.first);
         //printf("***%s\n", p.second.c_str());
         int expanded = indigoLoadMoleculeFromString(p.second.c_str());
         //printf("***%d\n", expanded);
         
         int attachment = 0;

         iter = indigoIterateAtoms(expanded);
         while (item = indigoNext(iter))
         {
            if (item == -1)
               throw LogicException("%s", indigoGetLastError());
            
            if (indigoIsRSite(item))
            {
               int item2, iter2 = indigoIterateNeighbors(item);
               while (item2 = indigoNext(iter2))
               {
                  if (item2 == -1)
                     throw LogicException("%s", indigoGetLastError());
   
                  attachment = item2;
                  
                  //Only 1 attachment point. So, no freeing here
                  //indigoFree(item2);
               }      
               indigoFree(iter2);
               
               indigoRemove(item);
               break;
            }
            indigoFree(item);          
         }
         indigoFree(iter);
         
         int item2, iter2 = indigoIterateNeighbors(p.first);
         int atom_from, bond_order;
         while (item2 = indigoNext(iter2))
         {
            if (item2 == -1)
               throw LogicException("%s", indigoGetLastError());         

            atom_from = item2;
            bond_order = indigoBondOrder(indigoBond(atom_from));
         }
         indigoFree(iter2);
         
         //printf("***%d\n", bond_order);
         
         float xyz[3];
         memcpy(xyz, indigoXYZ(p.first), 3 * sizeof(float));
         indigoRemove(p.first);
       
         int mapping = indigoMerge(mol, expanded);
         int mapped = indigoMapAtom(mapping, attachment);

         indigoAddBond(atom_from, mapped, bond_order);
         indigoSetXYZ(mapped, xyz[0], xyz[1], xyz[2]);
         
         vector<int> superatom_vertices;
         iter = indigoIterateAtoms(expanded);
         while (item = indigoNext(iter))
         {
            if (item == -1)
               throw LogicException("%s", indigoGetLastError());         
            
            int midx = indigoIndex(indigoMapAtom(mapping, item));
            superatom_vertices.push_back(midx);
            
            int attach_index = indigoIndex(attachment);
            if (indigoIndex(item) != attach_index)
            {
               to_layout.push_back(midx);
            }
         }
         indigoFree(iter);
         
         indigoAddSuperatom(mol, superatom_vertices.size(), &superatom_vertices[0], name);
         //indigoFree(attachment);
         //indigoFree(atom_from);
         //indigoFree(mapped);
         //indigoFree(mapping);
      }
      
      int subm = indigoGetSubmolecule(mol, to_layout.size(), &to_layout[0]);
      indigoLayout(subm);
      
      string newMolfile = indigoMolfile(mol);
      //puts(newMolfile.c_str());
      //indigoFree(subm);
      //indigoFree(mol);
      return newMolfile;
   }
}