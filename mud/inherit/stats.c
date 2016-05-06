/** Primarily intended to be inherited by living. Code to keep track
 * of primary statistics (str, con, dex, spd, wil, int, cha, emp)
 * is in here.
 */

#define DECAY_DAMPER 100

/// Stats (str, dex, int, etc...) stored in this object.
mapping stats;
/// Temporary bonus applied to stats
nosave mapping stat_bonus;
/// How much does this stat get used?
mapping stat_use;

/// \return The total bonus added to a particular stat.
int query_stat_bonus(string stat) {
   if( !mappingp(stat_bonus) ) stat_bonus = ([ ]);
   return stat_bonus[stat];
}

/// Return a stat, including any bonus.
int query_stat(string x) {
   int bonus;
   int value;

   if( !mappingp(stat_bonus) ) stat_bonus = ([ ]);
   bonus = stat_bonus[x];
   if (!stats)
      stats = (["con":10, "str":10, "dex":10, "spd":10, "wil":10, "cha":10,
                "int":10, "emp":10]);
   value = stats[x] + bonus;
   // Bonus alone may not drop a stat below 1.
   if( -bonus < value )
      return value;
   else
      return 1;
}

/// Return a stat, without the bonus added in.
int query_real_stat(string x) {
   int bonus;
   int value;

   if (!stats)
      stats = (["con":10, "str":10, "dex":10, "spd":10, "wil":10, "cha":10,
                "int":10, "emp":10]);
   return stats[x];
}

/// Sets the base value of a stat.
void set_stat( string stat, int x ) {
   if (!stats)
      stats = (["con":10, "str":10, "dex":10, "spd":10, "wil":10, "cha":10,
                "int":10, "emp":10]);

   if( x == 0 )
      m_delete( stats, stat );
   else
      stats[stat] = x;

   // Make usage match the setting
   stat_use[stat] = x * 100;

   // If con changes, may need to start healing...
   if( stat == "con" ) set_heart_beat( 1 );
}

void set_stat_use( string stat, int x ) {
   if( !stat_use )
      stat_use = (["con":1000, "str":1000, "dex":1000, "spd":1000, "wil":1000, "cha":1000,
                "int":1000, "emp":1000]);

   if( x == 0 )
      m_delete( stat_use, stat );
   else
      stat_use[stat] = x;
}

void recalc_stats() {
   string istat;
   object race;

   // Should never happen, but shrug.
   if( !stats ) return;

   // Determine race to query for stat maximums
   race = this_object()->query_race_object();
   if (!race)
      race = find_object("/race/generic.c");

   // Decay
   foreach( istat : stat_use ) {
      if( stat_use[istat] < stats[istat] * 100 - 50 )
         stats[istat] = (stat_use[istat] + 50) / 100;
   }

   // Growth
   foreach( istat : stat_use ) {
      if( istat == "will" ) {
         stat_use["wil"] = stat_use["will"];
         stats["wil"] = stat_use["will"];
         m_delete( stat_use, istat );
         m_delete( stats, istat );
      }
      else
         if( stat_use[istat] >= stats[istat] * 100 ) {
            int stat_max = race->query_stat_max(istat);
            if( stat_use[istat] > stat_max * 100 ) stat_use[istat] = stat_max * 100;
            if (this_object()->query_is_player() && stat_use[istat] / 100 >= stat_max)
               stats[istat] = stat_max;
            else if (this_object()->query_is_player() && stat_use[istat] / 100 < 1)
               stats[istat] = 1;
            else
               stats[istat] = stat_use[istat] / 100;
         }
   }
}

void add_stat_use( string stat, int x ) {
   string istat;

   if( !stat_use )
      stat_use = (["con":1000, "str":1000, "dex":1000, "spd":1000, "wil":1000, "cha":1000,
                "int":1000, "emp":1000]);

   if( !member(stat_use, stat) ) return;
   stat_use[stat] += x;

   recalc_stats();
}

void _reset_stats() {
   int istat, tot;
   string statkey;

   if( !stat_use || sizeof(stat_use) < 8 )
      stat_use = (["con":1000, "str":1000, "dex":1000, "spd":1000, "wil":1000, "cha":1000,
                "int":1000, "emp":1000]);

   tot = 0;
   foreach( statkey, istat : stat_use ) {
      tot += istat;
   }

// NOTE: Decay is currently turned off. It didn't have the effect I wanted. -- Acius
   // Dampen the decay ... the higher the dampener (the denominator), the slower things will decay.
//   tot = 8000 + (tot - 8000) / DECAY_DAMPER;

   // Total can range from 8000 (no decay) to 8000 + (8000 / DECAY_DAMPER)
   // Note that if your stats total is less than 8000, this actually GROWS your stats.
//   foreach( statkey : stat_use )
//      stat_use[statkey] = stat_use[statkey] * 8000 / tot;

   recalc_stats();
}

mapping query_stats() {
   return stats;
}

mapping query_stats_bonus() {
   return stat_bonus;
}

mapping query_stats_use() {
   return stat_use;
}

/// \return The total bonus added to speed.
int query_speed_bonus() { return query_stat_bonus("spd");  }
/// \return The total bonus added to speed.
int query_spd_bonus() { return query_stat_bonus("spd"); }

/// \return The total bonus added to constitution.
int query_constitution_bonus() { return query_stat_bonus("con"); }
/// \return The total bonus added to constitution.
int query_con_bonus() { return query_stat_bonus("con"); }

/// \return The total bonus added to strength.
int query_strength_bonus() { return query_stat_bonus("str");  }
/// \return The total bonus added to strength.
int query_str_bonus() { return query_stat_bonus("str"); }

/// \return The total bonus added to will.
int query_will_bonus() { return query_stat_bonus("wil");  }
/// \return The total bonus added to will.
int query_wil_bonus() { return query_stat_bonus("wil"); }

/// \return The total bonus added to dexterity.
int query_dexterity_bonus() { return query_stat_bonus("dex");  }
/// \return The total bonus added to dexterity.
int query_dex_bonus() { return query_stat_bonus("dex"); }

/// \return The total bonus added to intelligence.
int query_intelligence_bonus() { return query_stat_bonus("int");  }
/// \return The total bonus added to intelligence.
int query_int_bonus() { return query_stat_bonus("int"); }

/// \return The total bonus added to empathy.
int query_empathy_bonus() { return query_stat_bonus("emp");  }
/// \return The total bonus added to empathy.
int query_emp_bonus() { return query_stat_bonus("emp"); }

/// \return The total bonus added to charm.
int query_charm_bonus() { return query_stat_bonus("cha");  }
/// \return The total bonus added to charm.
int query_cha_bonus() { return query_stat_bonus("cha"); }

/// \return This object's speed, including the bonus.
int query_speed() { return query_stat( "spd" ); }
/// \return This object's speed, including the bonus.
int query_spd() { return query_stat( "spd" ); }

/// \return This object's constitution, including the bonus.
int query_constitution() { return query_stat("con"); }
/// \return This object's constitution, including the bonus.
int query_con() { return query_stat("con"); }

/// \return This object's strength, including the bonus.
int query_strength() { return query_stat("str"); }
/// \return This object's strength, including the bonus.
int query_str() { return query_stat("str"); }

/// \return This object's will, including the bonus.
int query_will() { return query_stat("wil"); }
/// \return This object's will, including the bonus.
int query_wil() { return query_stat("wil"); }

/// \return This object's dexterity, including the bonus.
int query_dexterity() { return query_stat("dex"); }
/// \return This object's dexterity, including the bonus.
int query_dex() { return query_stat("dex"); }

/// \return This object's intelligence, including the bonus.
int query_intelligence() { return query_stat("int"); }
/// \return This object's intelligence, including the bonus.
int query_int() { return query_stat("int"); }

/// \return This object's empathy, including the bonus.
int query_empathy() { return query_stat("emp"); }
/// \return This object's empathy, including the bonus.
int query_emp() { return query_stat("emp"); }

/// \return This object's charm, including the bonus.
int query_charm() { return query_stat("cha"); }
/// \return This object's charm, including the bonus.
int query_cha() { return query_stat("cha"); }

/// \return This object's base speed, NOT including any bonuses.
int query_real_speed() { return query_real_stat("spd"); }
/// \return This object's base speed, NOT including any bonuses.
int query_real_spd() { return query_real_stat("spd"); }

/// \return This object's base constitution, NOT including any bonuses.
int query_real_constitution() { return query_real_stat("con"); }
/// \return This object's base constitution, NOT including any bonuses.
int query_real_con() { return query_real_stat("con"); }

/// \return This object's base strength, NOT including any bonuses.
int query_real_strength() { return query_real_stat("str"); }
/// \return This object's base strength, NOT including any bonuses.
int query_real_str() { return query_real_stat("str"); }

/// \return This object's base will, NOT including any bonuses.
int query_real_will() { return query_real_stat("wil"); }
/// \return This object's base will, NOT including any bonuses.
int query_real_wil() { return query_real_stat("wil"); }

/// \return This object's base dexterity, NOT including any bonuses.
int query_real_dexterity() { return query_real_stat("dex"); }
/// \return This object's base dexterity, NOT including any bonuses.
int query_real_dex() { return query_real_stat("dex"); }

/// \return This object's base intelligence, NOT including any bonuses.
int query_real_intelligence() { return query_real_stat("int"); }
/// \return This object's base intelligence, NOT including any bonuses.
int query_real_int() { return query_real_stat("int"); }

/// \return This object's base empathy, NOT including any bonuses.
int query_real_empathy() { return query_real_stat("emp"); }
/// \return This object's base empathy, NOT including any bonuses.
int query_real_emp() { return query_real_stat("emp"); }

/// \return This object's base charm, NOT including any bonuses.
int query_real_charm() { return query_real_stat("cha"); }
/// \return This object's base charm, NOT including any bonuses.
int query_real_cha() { return query_real_stat("cha"); }

/// Set this living's speed, one of the eight base stats.
void set_speed(int x) { set_stat("spd", x); }
/// Set this living's speed, one of the eight base stats.
void set_spd(int x) { set_stat( "spd", x ); }

/// Set this living's constitution, one of the eight base stats.
void set_constitution(int x) { set_stat( "con", x ); }
/// Set this living's constitution, one of the eight base stats.
void set_con(int x) { set_stat( "con", x ); }

/// Set this living's strength, one of the eight base stats.
void set_strength(int x) { set_stat( "str", x );  }
/// Set this living's strength, one of the eight base stats.
void set_str(int x) { set_stat( "str", x ); }

/// Set this living's will, one of the eight base stats.
void set_will(int x) { set_stat( "wil", x );  }
/// Set this living's will, one of the eight base stats.
void set_wil(int x) { set_stat( "wil", x ); }

/// Set this living's intelligence, one of the eight base stats.
void set_intelligence(int x) { set_stat( "int", x );  }
/// Set this living's intelligence, one of the eight base stats.
void set_int(int x) { set_stat( "int", x ); }

/// Set this living's dexterity, one of the eight base stats.
void set_dexterity(int x) { set_stat( "dex", x );  }
/// Set this living's dexterity, one of the eight base stats.
void set_dex(int x) { set_stat( "dex", x ); }

/// Set this living's empathy, one of the eight base stats.
void set_empathy(int x) { set_stat( "emp", x );  }
/// Set this living's empathy, one of the eight base stats.
void set_emp(int x) { set_stat( "emp", x ); }

/// Set this living's charm, one of the eight base stats.
void set_charm(int x) { set_stat( "cha", x );  }
/// Set this living's charm, one of the eight base stats.
void set_cha(int x) { set_stat( "cha", x ); }
