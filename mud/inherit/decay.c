/** The purpose of this object is to handle decay. It is quite
 * gentle on the CPU, so don't have qualms about inheriting it
 * everywhere. It uses the reset() event to decay.
 *
 * The default settings for Walraven as of this writing are one
 * reset approximately every hour; the LDMud driver randomizes
 * this somewhat, so only expect this as an average.
 *
 * The value of 'perishable' determines how many resets an item lasts
 * before decaying.  If 0, the item doesn't decay.  For any other value,
 * the item will decay after 'perishable' number of resets. For
 * comparison, there are 168 hours in a week, ~720 hours in a month,
 * and ~8760 in a year; an item with a perishable value of 1 should
 * self-destruct after an hour.
 *
 * This object triggers the on_decay(int) event every time decay occurs.
 * For information on that, read the on_decay man page.
 */

int decay;

/// Sets the decay speed to \a x.
void set_perishable( int x ) { SBODY("perishable", x) }

/// Sets the amount of decay to \a x.
varargs void set_decay( int x, varargs int *index ) {
   if( this_object()->query_is_quantity() && this_object()->query_quantity() < 10000 ) {
      if( sizeof(index) )
         this_object()->set_qvar( "decay", x, index[0] );
      else {
         int i;
         for( i = this_object()->query_quantity(); i--; )
            this_object()->set_qvar( "decay", x, i );
      }
   }
   else
      decay = x;
}

/// \return The speed of the object's decay.
int query_perishable() { QBODY("perishable", 0) }

/// \return The amount the object has decayed.
varargs int query_decay( varargs int *index ) {
   if( this_object()->query_is_quantity() && this_object()->query_quantity() < 10000 ) {
      int decay_tot;
      if( !sizeof(index) ) {
         int i;
         for( i = this_object()->query_quantity(); i--; ) {
            decay_tot += this_object()->query_qvar( "decay", i );
         }
         if ( this_object()->query_quantity() == 0 )
            return decay_tot / 1;
         else
            return decay_tot / this_object()->query_quantity();
      }
      else
         return this_object()->query_qvar( "decay", index[0] );
   }
   return decay;
}

/// This function is for keeping objedit happy
string decay_info() {
  return "perishable\ndecay";
}

/// Decays the object, triggering a self-destruct event when it's done.
void _reset_decay() {
   if( !query_perishable() ) return;
   set_decay( query_decay() + 1 );
   this_object()->on_decay( query_decay() );
   if( query_decay() >= query_perishable() ) call_out( "decay_destruct", 0 );
}

/// Destruct this object. Triggered from _reset_decay().
static void decay_destruct() {
   destruct( this_object() );
}
