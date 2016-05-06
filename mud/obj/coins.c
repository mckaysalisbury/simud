//
// Coded By      : Silvermane (sylvermane@hotmail.com)
// Created On    : 20th Jan 2002
// Last Modified : 20th Jan 2002 by Silvermane
// De-stupified  : 9th Jul 2004 by Malap

#include <const.h>
#include <saveload.h>
#include <desc.h>
#include <object.h>
#include <item.h>
#include <quantity.h>
#include <stackobj.c>

NAME( "coin" )
DISTANT( "a coin" )
SPECIFIC( "the coin" )
LOOK( "A shiny little coin." )
PLURAL( "coins" )
TYPE( "coin" )
GETTABLE( 1 )
DROPPABLE( 1 )
WEIGHT( 1 )
BULK( 30 )
VALUE( 1 )		// by definition, really

ICON( '$', 0x8B, LAYER_ITEMS )

void create() {
   ::create();
   set_quantity( 1 );
}

/// Indicates that this object is usable as money.
int query_is_money() {
   return 1;
}
