#include <edible.h>
#include <item.h>
#include <object.h>
#include <saveload.h>
#include <decay.h>
#include <condition.h>

// Removed this on principle that decay and quantity do not play nice
//  - Al [9.9.04]
// They should now, putting it back -- Acius [2004.09.12]
#include <quantity.h>

// Removed this one as well, and moved it up from the bottom of the bloody file
// ... it was at the bottom because it's not a .h file. It certainly must
// NOT be above the struct declaration or you'll break stuff that won't
// be noticed for a while. Note the .c -- this is not a header file.
#include <stackobj.c>

PERISHABLE( 1500 ) // About a month to decay

void create() {
   ::create();
/* OBJEDIT { */
   set_quantity( 1 );
   set_max_condition( 5 );
   set_dest_on_break( 1 );
/* } OBJEDIT */
}

void on_paint(object painter) {
  painter->paint(query_x(),query_y(),'%',3,LAYER_ITEMS);
}

// Default behavior: Eat the entire piece of food and self-destruct.
int on_consume( int max ) {
   default_consume(max);
   return 1;
}

void on_decay( int decay ) {
   if( !environment() ) return;
   if( decay == query_perishable() / 2 ) {
      set_actor(this_object());
      msg_room(environment(), "~CACT~Name ~verbis getting noticeably less fresh." );
   }
   if( decay == query_perishable() * 3 / 4 ) {
      set_actor(this_object());
      msg_room(environment(), "~CACT~Name ~verbstart smelling bad." );
   }
   if( decay == query_perishable() ) {
      set_actor(this_object());
      msg_room(environment(), "~CACT~Name ~verbdecay away to nothing." );
   }
}
