//
//  KetcherViewController.h
//  ireco
//
//  Created by Boris Karulin on 07.12.10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "chemical_structure_recognizer.h"
#import "Recognizer.h"

@interface KetcherViewController : UIViewController <UINavigationControllerDelegate, UIWebViewDelegate>
{
   UIWebView *webView;
   UIActivityIndicatorView *activityView;
@private
   NSString *molfile;
   UIImage *prevImage;
   Recognizer *recognizer;
   NSThread *recognizerThread;
   BOOL keepAlive;
}    

@property (nonatomic, retain) IBOutlet UIWebView *webView;
@property (nonatomic, retain) IBOutlet UIActivityIndicatorView *activityView;
@property (nonatomic, retain) NSString *molfile;
@property (nonatomic, retain) UIImage *prevImage;
@property (nonatomic, retain) Recognizer *recognizer;
@property (nonatomic, retain) NSThread *recognizerThread;

- (void)setupKetcher:(UIImage *)image;
- (void)recognizerThreadProc;
- (void)recognizingProc:(UIImage *)image;

@end
