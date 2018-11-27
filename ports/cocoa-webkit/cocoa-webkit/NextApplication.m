//
//  NextApplication.m
//  next-cocoa
//
//  Created by John Mercouris on 3/25/18.
//  Copyright © 2018 Next. All rights reserved.
//

#import "NextApplication.h"
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/config.h>
#include "Global.h"

@implementation NextApplication

- (void)sendEvent:(NSEvent *)event
{
    if ([event type] == NSEventTypeKeyDown) {
        NSEventModifierFlags modifierFlags = [event modifierFlags];
        NSString* characters = [event charactersIgnoringModifiers];
        short keyCode = [event keyCode];
        bool controlPressed = (modifierFlags & NSEventModifierFlagControl);
        bool alternatePressed = (modifierFlags & NSEventModifierFlagOption);
        bool commandPressed = (modifierFlags & NSEventModifierFlagCommand);
        bool functionPressed = (modifierFlags & NSEventModifierFlagFunction);
        
        xmlrpc_env env = [[Global sharedInstance] getXMLRPCEnv];
        xmlrpc_value * resultP;
        xmlrpc_value * modifiers;
        xmlrpc_int consumed;
        modifiers = xmlrpc_array_new(&env);
        
        if (controlPressed) {
            xmlrpc_value * itemP;
            itemP = xmlrpc_string_new(&env, "C");
            xmlrpc_array_append_item(&env, modifiers, itemP);
            xmlrpc_DECREF(itemP);
        };
        if (alternatePressed) {
            xmlrpc_value * itemP;
            itemP = xmlrpc_string_new(&env, "M");
            xmlrpc_array_append_item(&env, modifiers, itemP);
            xmlrpc_DECREF(itemP);
        };
        if (commandPressed) {
            xmlrpc_value * itemP;
            itemP = xmlrpc_string_new(&env, "S");
            xmlrpc_array_append_item(&env, modifiers, itemP);
            xmlrpc_DECREF(itemP);
        };
        if (functionPressed) {
            xmlrpc_value * itemP;
            itemP = xmlrpc_string_new(&env, "F");
            xmlrpc_array_append_item(&env, modifiers, itemP);
            xmlrpc_DECREF(itemP);
        };
        
        // Make the remote procedure call
        resultP = xmlrpc_client_call(&env, "http://localhost:8081/RPC2", "PUSH-KEY-CHORD",
                                     "(isA)",
                                     (xmlrpc_int) keyCode,
                                     [characters UTF8String],
                                     modifiers);
        xmlrpc_read_int(&env, resultP, &consumed);

        xmlrpc_DECREF(modifiers);
        xmlrpc_DECREF(resultP);
        
        if (consumed) {
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
                xmlrpc_env env = [[Global sharedInstance] getXMLRPCEnv];
                xmlrpc_client_call(&env, "http://localhost:8081/RPC2", "CONSUME-KEY-SEQUENCE", "(i)", (xmlrpc_int) 1);
            });
        } else {
            [super sendEvent:event];
        }
    } else {
        [super sendEvent:event];
    }
}

@end
