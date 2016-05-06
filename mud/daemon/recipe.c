/** This object contains some utility functions to check recipe matching, and
 * may also be used to store recipes.
 */

mapping recipes;
mapping diff;

/// Debug function
mapping query_recipes() {
   return recipes;
}

mapping query_diff() {
   return diff;
}

mapping query_aspect_difficulty(int aspect) {
   return diff[aspect];
}

/** A function that's likely to be killed. Only useful for mixing dough.
 */
int mix_contents( object container ) {
   object ob1, ob2, ob3;

   if( (ob1 = present("flour", container)) && (ob2 = present("water", container)) && (ob3 = present("yeast", container)) ) {
      object dough;
      if( ob1->query_quantity() * 2 / 3 < ob2->query_quantity() ) {
         msg("The paste won't bind, there's too much water.");
         return 1;
      }
      dough = clone_object( "/items/food/dough" );
      dough->move( container );
      dough->set_quantity( ob1->query_quantity() + ob2->query_quantity() + ob3->query_quantity() );
      destruct(ob1);
      destruct(ob2);
      destruct(ob3);
      set_target( container );
      this_player()->msg_local("~Name ~verbknead the flour and water in ~targ into a thick dough.");
      return 1;
   }
   return 0;
}

/** Pass it a recipe in this format:
 *
 * ({ ingredient1, ingredient2, ... })
 *
 * where an ingredient is a two-item array:
 *
 * ({ classification, quantity })
 *
 * Quantity is an int, and classification is a mapping of classifications, similar
 * to what query_aspects() returns in most objects.
 *
 * This returns a list of all the objects used in the recipe, with quantities already
 * split, which you should move somewhere immediately (to avoid having the quantities
 * recombine), or destroy, depending on context. If the ingredients did not match,
 * then it will return 0.
 *
 * \warning Do not pass a large list of potential ingredients in, or this function
 * will be very slow! Try to limit it down to likely candidates only.
 */
object *query_recipe_match( mixed *recipe, object *available, int pure ) {
   int irecipe, iing, multiplier;
   object *used;

   used = ({ });
   recipe = deep_copy(recipe);

   for( iing = sizeof(available); iing--; ) {
      for( irecipe = sizeof(recipe); irecipe--; ) {
         mapping class;
         int q;
         object ob;

         ob = available[iing];
       if (!ob) continue;
         class = ob->query_aspects();
         if( !class || sizeof(recipe[irecipe][0] - class) ) continue;
         // Three cases for quantity...
         q = ob->query_quantity();
         if( !q ) q = 1;
         // Abundance!
         set_target(ob);
         if( recipe[irecipe][1] < q ) {
            ob->split_quantity( recipe[irecipe][1] );
            used += ({ ob });
            recipe -= ({ recipe[irecipe] });
            break;
         }
         // Just right
         else if( recipe[irecipe][1] == q ) {
            used += ({ ob });
            recipe -= ({ recipe[irecipe] });
            break;
         }
         else if( !pure ) { // Not enough in the ingredient
            recipe[irecipe][1] -= q;
            used += ({ ob });
            break;
         }
      }
   }
   if( sizeof(recipe) == 0 )
      return used;
   return 0;
}

mapping query_recipe( string name ) {
   return deep_copy(recipes[name]);
}

string query_recipe_category( string name ) {
   return query_recipe(name)["category"];
}

#define ASPECT_STR(x) ( "/daemon/material"->query_aspect_string(x) )

varargs int craft( string name, object *materials, object container ) {
   mapping recipe;
   object ob, *ingredients;
   int bonus_practices = 0;

   recipe = query_recipe( name );

   if( container && container->query_weight_free() < recipe["liquid"] ) {
      set_target(container);
      msg("~Targ is either too small or too full for this.");
      return 1;
   }

   if( !materials ) materials = deep_inventory(this_player());

   if( !recipe || (recipe["type"] && recipe["type"] != "item") ) return 0;

   ingredients = recipe["ingredients"];

   if( ingredients && materials = query_recipe_match(ingredients, materials, recipe["pure"]) ) {
      object imat;
      string skill;
      mapping prac_skills = ([ ]);
      int aspect, failure = 0;
      int target_aspect = 0;

      // make sure the player has sufficient skill to work with the material at hand
      foreach( imat : materials ) {
         bonus_practices += max( 1, imat->query_quantity() / 40 );
         foreach( aspect : imat->query_aspects() ) {
            mapping diff_map = query_aspect_difficulty( aspect );
            // determine target aspect if appropriate
            if( recipe["aspect"] ) {
               if( member( recipe["aspect"], aspect ) ) {
                  if( !target_aspect ) {
                     target_aspect = aspect;
                     msg("You will be using ~CCOM"+ASPECT_STR(target_aspect)+"~CDEF for the job.");
                  } else
                  if( target_aspect != aspect && recipe["pure"] ) {
                     msg("Oops! This recipe doesn't allow for hybrid ingredients. You are already using ~CCOM" + ASPECT_STR(target_aspect) + "~CDEF, but ~targ is made of ~CCOM" + ASPECT_STR(aspect) + "~CDEF.");
                     failure = 1;
                     continue;
                  }
               }
            }
            // ok, _now_ we check difficulty
            if( !mappingp(diff_map) )
               continue;
            foreach( skill : m_indices(diff_map) ) {
               int check = this_player()->query_skill(skill) - diff_map[skill];
               if( check < 0 ) {
                  set_target(imat);
                  msg("You need more ~CTIT"+skill+"~CDEF to craft with ~CTIT~targ~CDEF.");
                  failure = 1;
               } else if( check < 25 ) {
                  // only give pracs for working in materials you're not overqualified for
                  prac_skills[skill] = imat->query_quantity() / 40 + 1;
               }
            }
         }
      }
      if( failure )
         return 1;

     // if an aspect is required, check if one was selected
     if( recipe["aspect"] && member(recipe["aspect"], target_aspect) == -1 ) {
         string buf = "";
         foreach( aspect : recipe["aspect"] )
            buf += ASPECT_STR(aspect) + ", ";
         buf = buf[<3..];
         msg("You must use materials that provide one of the aspects listed in the recipe:\n    "+buf);
         return 1;
     }

      int amt;
      // practice if they didn't fail
      foreach( skill, amt : prac_skills )
         this_player()->practice_skill( skill, amt );
      foreach( skill : m_indices(recipe["skills"]) ) {
         this_player()->skill_check(skill, recipe["skills"][skill]);
         this_player()->practice_skill( skill, bonus_practices );
      }
      // TODO: allow for exceptional and poor results based on these checks

      // actually do the crafting
      ob = funcall( recipe["output"] );
      if( !ob ) {
         msg("The crafting did nothing!");
         return 1;
      }
     if( container )
         ob->move(container);
      else
         ob->move(this_player());
      ob->set_owner( this_player()->query_name() );
      if( !ob->on_craft(materials) ) {
         // one last thing before we go
         ob->apply_aspect( target_aspect );
       // ok, now message an dest materials
         set_target(ob);
       // msg appropriately about your container ;)
       if( container )
            this_player()->msg_local("~CACT~Name ~verbcraft ~targ into "+container->query_specific()+".~CDEF");
       else
            this_player()->msg_local("~CACT~Name ~verbcraft ~targ.~CDEF");
         foreach( imat : materials )
            destruct( imat );
      }
      else {
         foreach( imat : materials )
            imat->move( ob );
      }
      return 1;
   }
   else {
      msg("You don't have the required materials to do it.");
      return 1;
   }

}

/** Returns the total number of materials used (simple summation)
 */
varargs int check_resources( string name, object *materials, object craft_ob ) {
   mapping recipe;
   object *ingredients;
   int res_count = 0;

   recipe = query_recipe( name );
   if( !materials ) materials = all_inventory(this_player());

   if( !recipe ) {
      debug("check_resources("+name+"): No such thing.");
      return 0;
   }
   ingredients = deep_copy(recipe["ingredients"]);
   if( ingredients && materials = query_recipe_match(ingredients, materials, recipe["pure"]) ) {
      object imaterial;
      int move;

      move = 0;
      foreach( imaterial : materials )
         res_count += imaterial->query_quantity();
      if( res_count == 0 ) res_count = 1;
      if( objectp(craft_ob) && craft_ob->on_craft(materials) )
         move = 1;
      foreach( imaterial : materials ) {
         if( !move )
            destruct(imaterial);
         else
            imaterial->move(craft_ob);
      }
      return res_count;
   }
   else
      return 0;
}

varargs object get_building( string name ) {
   mapping recipe;
   object ob;
   string skill;
   int error = 0;

   recipe = query_recipe( name );

   if( !recipe || recipe["type"] != "structure" ) {
      msg("No such building.");
      return 0;
   }
   // check for skills first
   if( mappingp(recipe["skills"]) ) {
      foreach ( skill : m_indices(recipe["skills"]) ) {
         int x = this_player()->query_skill(skill);
         if (!x) {
            msg("You need to have the ~CTIT"+skill+"~CDEF skill in order to build that.");
            error = 1;
         }
      }
   }
   else
      debug( "Warning: recipe " + name + " has no valid skills listed. At least provide an empty mapping." );
   // then try to make the object if all goes well
   if (!error) {
     ob = funcall( recipe["output"] );
     if( !ob ) {
        msg("There was a problem when trying to create the building.");
        return 0;
     }
     ob->move(environment(this_player()));
   }
   return ob;
}

void add_recipe( string key, mapping data ) {
   string cat;
   if( !stringp(key) || !mappingp(data) ) {
      debug("A recipe addition from " + as_string(previous_object()) + " was invalid");
      return;
   }
   cat = explode(as_string(previous_object()),"/")[<1];
   recipes[key] = data + ([ "category" : cat ]);
}

void create() {
   string obname;

   recipes = ([ ]);
   foreach( obname : get_dir("/econ/recipes/*.c") ) {
      if( !find_object("/econ/recipes/" + obname) )
         catch(load_object( "/econ/recipes/" + obname ));
      else
         catch(call_other( "/econ/recipes/" + obname, "create" ));
   }

   // this just gets hard coded into the daemon
   diff = ([
        // metals
        C_ADAMANT:    ([ "craft.smithing.black":100 ]),
        C_ALUMINUM:   ([ "craft.smithing.white":75 ]),
        C_BRASS:      ([ "craft.smithing.black":50,
                         "craft.smithing.white":50 ]),
        C_BRONZE:     ([ "craft.smithing.black":25,
                         "craft.smithing.white":25 ]),
        C_COPPER:     ([ "craft.smithing.black":1 ]),
        C_ELECTRUM:   ([ "craft.smithing.white":75 ]),
        C_GOLD:       ([ "craft.smithing.white":75 ]),
        C_H_STEEL:    ([ "craft.smithing.black":75 ]),
        C_IRON:       ([ "craft.smithing.black":50 ]),
        C_LEAD:       ([ "craft.smithing.white":1 ]),
        C_MITHRIL:    ([ "craft.smithing.black":75,
                         "craft.smithing.white":50 ]),
        C_NICKEL:     ([ "craft.smithing.black":50 ]),
        C_ORICHALCUM: ([ "craft.smithing.white":100,
                         "magic.mana.fire":50,
                         "magic.mana.earth":50,
                         "magic.mana.wind":50,
                         "magic.mana.water":50 ]),
        C_PEWTER:     ([ "craft.smithing.white":25 ]),
        C_PLATINUM:   ([ "craft.smithing.white":75 ]),
        C_SILVER:     ([ "craft.smithing.white":50 ]),
      C_STEEL:      ([ "craft.smithing.black":75 ]),
        C_TIN:        ([ "craft.smithing.white":1 ]),
        C_W_GOLD:     ([ "craft.smithing.white":100 ]),
        C_ZINC:       ([ "craft.smithing.white":50 ]),
        // woods
        C_APPLEWOOD:    ([ "craft.carpentry.furniture":25 ]),
        C_ASHWOOD:      ([ "craft.carpentry.furniture":75 ]),
      //C_BAMBOO:       ([ "craft.carpentry.furniture":1 ]),
        C_BIRCHWOOD:    ([ "craft.carpentry.furniture":25 ]),
        C_CEDARWOOD:    ([ "craft.carpentry.furniture":25 ]),
        C_CHERRYWOOD:   ([ "craft.carpentry.furniture":75 ]),
        C_CHESTNUTWOOD: ([ "craft.carpentry.furniture":50 ]),
        C_ELMWOOD:      ([ "craft.carpentry.furniture":1 ]),
        C_FIRWOOD:      ([ "craft.carpentry.furniture":1 ]),
        C_HICKORYWOOD:  ([ "craft.carpentry.furniture":75 ]),
        C_IRONWOOD:     ([ "craft.carpentry.furniture":100 ]),
        C_LEMONWOOD:    ([ "craft.carpentry.furniture":25 ]),
        C_LIMEWOOD:     ([ "craft.carpentry.furniture":25 ]),
        C_MAHOGANYWOOD: ([ "craft.carpentry.furniture":75 ]),
        C_MAPLEWOOD:    ([ "craft.carpentry.furniture":50 ]),
        C_OAKWOOD:      ([ "craft.carpentry.furniture":50 ]),
        C_OLIVEWOOD:    ([ "craft.carpentry.furniture":100 ]),
        C_ORANGEWOOD:   ([ "craft.carpentry.furniture":25 ]),
        C_PEACHWOOD:    ([ "craft.carpentry.furniture":50 ]),
        C_PECANWOOD:    ([ "craft.carpentry.furniture":25 ]),
        C_PINEWOOD:     ([ "craft.carpentry.furniture":1 ]),
        C_PLUMWOOD:     ([ "craft.carpentry.furniture":25 ]),
        C_WALNUTWOOD:   ([ "craft.carpentry.furniture":50 ]),
        // textiles
   // gems
   ]);
}
