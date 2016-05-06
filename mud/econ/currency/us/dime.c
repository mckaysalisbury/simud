/* Based on /obj/coins */

#include <const.h>
#include <saveload.h>
#include <desc.h>
#include <object.h>
#include <item.h>
#include <quantity.h>
#include <stackobj.c>

NAME( "dime" )
ALT_NAME( ({ "coin" }) )
DISTANT( "a dime" )
SPECIFIC( "the dime" )
LOOK( "A tiny cupronickel coin with Franklin D. Roosevelt stamped on it.  A dime is worth 10 cents." )
PLURAL( "dimes" )
ALT_PLURAL( ({ "coins" }) )
TYPE( "coin" )
GETTABLE( 1 )
DROPPABLE( 1 )
WEIGHT( 3 )
BULK( 2 )
VALUE( 10 )      // 1p = 1p ;)

ICON( '$', 0x8B, LAYER_ITEMS )

void create() {
   ::create();
   set_quantity( 1 );
}

/// For lack of a better function name
string query_issuing_government() {
   return "united states";
}

/// Indicates that this object is usable as money.
int query_is_money() {
   return 1;
}

void on_split(object source)
{
   this_player()->msg_local("~CACTThe coins jingle as you manipulate them.~CDEF");
}
