nosave mapping room_paths;

/// Generates a routing table to allow NPC's to get around within an area.
void calc_room_paths() {
   object iroom, iexit, dest;
   string rname;
   int path_added;
   mapping room_exits, links;

   // Refuse to do this for large areas.
   if( sizeof(all_inventory()) > 30 ) raise_error("Area too large for route mapping (calc_room_paths)");

   room_paths = ([ ]);

   // Generates a simple routing table for room connections 1 deep
   foreach( iroom : filter(all_inventory(), (: $1->query_is_room() :)) ) {
      room_paths[iroom] = ([ ]);
      foreach( iexit : filter(all_inventory(iroom), (: $1->query_is_exit() :)) ) {
         dest = iexit->get_destination();
         if( environment(dest) != this_object() || !dest->query_is_room() ) continue;
         room_paths[iroom][dest] = iexit;
      }
   }

   // Every time you run this loop, the routing table grows one layer
   // deeper, until every possible interconnection is made.
   do {
      mapping room_diff;
      object idiff;

      path_added = 0;
      // Loop through all rooms in the area.
      foreach( iroom, room_exits : room_paths ) {
         if( sizeof(room_exits) == sizeof(room_paths) - 1 )
            continue;
         // Ask each of my adjacent rooms if they know how to get
         // to anywhere that I don't know how to get to.
         foreach( dest, iexit : room_exits ) {
            room_diff = (room_paths[dest] - room_exits) - ([ iroom ]);
            if( !sizeof(room_diff) ) continue;
            // OK, I can now go to those places by going through
            // that adjacent room. Since new routes were added,
            // I mark that I may need to run through this loop again.
            path_added = 1;
            foreach( idiff : room_diff )
               room_exits[idiff] = iexit;
         }
      }
   } while( path_added );
}

/// A recursive function used to help pretty-print the room mapping.
mapping obtoname( mixed m ) {
   mapping ret;
   mixed x, y;

   if( !mappingp(m) && !objectp(m) ) return m;
   if( objectp(m) ) return m->query_name();
   ret = ([ ]);
   foreach( x, y : m )
      ret += ([ obtoname(x):obtoname(y) ]);
   return ret;
}

/// Purely for debugging. Returns the paths in a human-readable form.
mapping query_name_room_paths() {
   return obtoname(room_paths);
}
