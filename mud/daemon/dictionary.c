#include <const/talkconst.h>

/** This file will handle some simple verb conjugation in English. It
 * is used by the simul_efun function process_string to do verb
 * conjugation, so you probably will not need to do anything here
 * directly. The reason it is in a separate file is so that you can
 * expand the verb capabilities without major difficulty. The verb
 * codes you should use are:
 *
 * ~Verb (present simple, upper case)
 * ~verb (present simple, lower case)
 * ~Vert (present simple, upper case, for target)
 * ~vert (present simple, lower case, for target)
 *
 * Example: ~Name ~verbpick up the rock.
 *
 * This phrase can be processed to both "You pick up the rock."
 * and "Bob picks up the rock." There are some other useful codes
 * you can use in these phrases. They are:
 *
 * - ~Targ -- The name of the target. If the target is the listener,
 * - then ~Targ will process to 'You' instead.
 * - ~Pron -- A pronoun for the subject of the sentence. Converts to 'you' if
 *   the subject is the listener, and he/she/it if not.
 * - ~Prot -- Same as ~Pron but for the target, not the subject.
 * - ~Poss -- Possessive for the subject, or 'your'.
 * - ~Post -- Same as ~Poss but for the target.
 *
 * All these have a lower case form.
 *
 * This will only handle a few irregulars. If you find an irregular verb
 * which is not handled properly, go ahead and add it in here.
 */

/** Conjugates an English verb in the present tense. This should
 * work for all English verbs; failing on any verb is considered a
 * bug and ought to be fixed.
 *
 * \param verb The verb to be conjugated
 * \param form 0 to conjugate in 3rd person (he, she, it verbs),
 * and 1 to conjugate in second person (you).
 *
 * There could be other useful forms, but given how simple present
 * tense English is, the second person should actually work for
 * basically all of them. If not, time to fix things...
 */
string conjugate_verb(string verb, int form) {
  if(!verb || verb == "") return "<null>";

  // Form 0 = 3rd person (he, she, it)
  if(!form) {
     if(verb == "is") return "is";
     if(verb == "survey") return "surveys";
     if(verb == "has" || verb == "have") return "has";
     if(verb == "go") return "goes";
     if(verb == "do") return "does";
     if(verb == "don't") return "doesn't";
     if(verb == "can") return "can";
     if(verb[<2..<1] == "gh") return verb + "s";
     if(verb[<2..<1] == "ay") return verb + "s";
     if(verb[<2..<1] == "th") return verb + "s";
     if(verb[<2..<1] == "uy") return verb + "s";
     if(verb[<1] == 'h') return verb + "es";
     if(verb[<1] == 'x') return verb + "es";
     if(verb[<1] == 's') return verb + "es";
     if(verb[<1] == 'y') return verb[0..<2] + "ies";
     return verb + "s";
  }
  // Form 1 = 2nd person (you)
  else {
    if(verb == "is") return "are";
    if(verb == "has") return "have";
    return verb;
  }
}

/** Queries the gender of \a ob, and returns an appropriate pronoun: you, it, he, or she.
 *
 * \param ob The object for which you need a pronoun. Uses Living::query_gender() (and assumes
 * neuter if query_gender() fails).
 * \param form If true, then returns "you" instead.
 */
string pronoun_for(object ob, int form) {
   int i;

   if(form) return "you";

   i = ob->query_gender();
   if(i == 0) return "it";
   if(i == 1) return "he";
   if(i == 2) return "she";
}

/** Queries the gender of \a ob, and returns an appropriate possessive pronoun: your, its, her, or his.
 *
 * \param ob The object for which you need a possessive pronoun. Uses Living::query_gender() (and assumes
 * neuter if query_gender() fails).
 * \param form If true, then returns "your" instead.
 */
string possessive_for(object ob, int form) {
   int i;

   if(form) return "your";

   i = ob->query_gender();
   if(i == 0) return "its";
   if(i == 1) return "his";
   if(i == 2) return "her";
}

/** Queries the gender of \a ob, and returns an appropriate reflexive pronoun: yourself, itself, herself, or himself.
 *
 * \param ob The object for which you need a reflexive pronoun. Uses Living::query_gender() (and assumes
 * neuter if query_gender() fails).
 * \param form If true, then returns "yourself" instead.
 */
string reflexive_for(object ob, int form) {
   int i;

   if(form) return "yourself";

   i = ob->query_gender();
   if(i == 0) return "itself";
   if(i == 1) return "himself";
   if(i == 2) return "herself";
}

/** Queries the gender of \a ob, and returns an appropriate object pronoun: you, it, her, or him.
 *
 * \param ob The object for which you need an object pronoun. Uses Living::query_gender() (and assumes
 * neuter if query_gender() fails).
 * \param form If true, then returns "you" instead.
 * \param actor If equal to \a ob, then use reflexives.
 */
string object_pronoun_for(object ob, int form, object actor) {
   int i;

   if( actor == ob ) return reflexive_for(ob, form);
   if(form) return "you";

   i = ob->query_gender();
   if(i == 0) return "it";
   if(i == 1) return "him";
   if(i == 2) return "her";
}

/// A small vocabulary for use by talking NPC's.
mapping noun_list, verb_list, adjective_list, preposition_list, exclamation_list;

/// Initialize the vocabulary lists. This object is never cloned, so I keep them here.
create() {
   noun_list =
// Times of day.
([ "day"      :OTIME,
   "night"    :OTIME,
   "morning"  :OTIME,
   "afternoon":OTIME,
   "evening"  :OTIME,
   "time"     :OTIME,
// Pronouns
   "i"        :NFIRSTP | NSUBJECT,
   "we"       :NFIRSTP | NPLURAL | NSUBJECT,
   "you"      :NSECONDP | NSUBJECT | NOBJECT,
   "he"       :NTHIRDP | NSUBJECT,
   "her"      :NTHIRDP | NOBJECT,
   "she"      :NTHIRDP | NSUBJECT,
   "it"       :NTHIRDP | NSUBJECT | NOBJECT,
   "they"     :NTHIRDP | NPLURAL | NSUBJECT,
   "them"     :NTHIRDP | NPLURAL | NOBJECT,
   "me"       :NFIRSTP | NOBJECT,
// Attributes
   "name"     :NATTRIB,
// Question words
   "what"     :NQUEST,
   "who"      :NQUEST,
   "how"      :NQUEST,
   "why"      :NQUEST,
   "where"    :NQUEST,
   "which"    :NQUEST
]); // End of noun list.
   verb_list =
([ "am"       :VBE,
   "are"      :VBE,
   "is"       :VBE,
   "have"     :VHAVE,
   "got"      :VHAVE,
   "go"       :VGO,
   "use"      :VUSE,
   "do"       :VACT,
   "want"     :VWANT,
   "like"     :VWANT,
   "wish"     :VWANT,
   "desire"   :VWANT,
   "need"     :VWANT,
   "buy"      :VBUY,
   "purchase" :VBUY,
   "give"     :VGIVE
]); // End of verb list.
   adjective_list =
([ "my"       :NFIRSTP | NPOSSESS,
   "our"      :NFIRSTP | NPOSSESS | NPLURAL,
   "your"     :NSECONDP | NPOSSESS,
   "his"      :NTHIRDP | NPOSSESS,
   "her"      :NTHIRDP | NPOSSESS,
   "its"      :NTHIRDP | NPOSSESS,
   "their"    :NTHIRDP | NPOSSESS | NPLURAL,
]);
   exclamation_list =
([ "yes"      :EYES,
   "yup"      :EYES,
   "yeah"     :EYES,
   "nod"      :EYES,
   "nog"      :EYES,
   "ok"       :EYES,
   "okay"     :EYES,
   "okey"     :EYES,
   "sure"     :EYES,
   "no"       :ENO,
   "nope"     :ENO
]); // End of exclamation list.
}

/// Fetch the noun list
mapping query_noun_list() {
   return noun_list;
}

/// Fetch the verb list
mapping query_verb_list() {
   return verb_list;
}

/// Fetch the adjective list
mapping query_adjective_list() {
   return adjective_list;
}

/// Fetch the exclamation list
mapping query_exclamation_list() {
   return exclamation_list;
}

static mapping numbers_map = ([ "one": 1, "two": 2, "three": 3, "four": 4, "five": 5,
   "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10,
   "eleven": 11, "twelve": 12, "thirteen": 13, "fourteen": 14,
   "fifteen": 15, "sixteen": 16, "seventeen": 17, "eighteen": 18,
   "nineteen": 19, "twenty": 20, "first": -1, "second": -2,
   "third": -3, "fourth": -4, "fifth": -5, "sixth": -6,
   "seventh": -7, "eighth": -8, "ninth": -9, "tenth": -10,
   "eleventh": -11, "twelfth": -12, "thirteenth": -13,
   "fourteenth": -14, "fifteenth": -15, "sixteenth": -16,
   "seventeenth": -17, "eighteenth": -18, "nineteenth": -19,
   "twentieth": -20, "both": 2, "zero": 0, "no": 0]);

/** Converts a string, s, from written form into numeric form.
 * For example, s="eight" will return the integer 8. Works for
 * numbers from one through twenty; the cardinal numbers, first
 * through twentieth, are returned as negative values. There are
 * also a few magical values: both=2, zero=0, no=0.
 *
 * This is intended for parsing.
 */
int get_number_from_string( string s ) {
   return numbers_map[s];
}

static mapping numbers_words_map = ([ 1: "one", 2: "two", 3: "three", 4: "four",
   5: "five", 6: "six", 7: "seven", 8: "eight", 9: "nine", 10: "ten",
   11: "eleven", 12: "twelve", 13: "thirteen", 14: "fourteen",
   15: "fifteen", 16: "sixteen", 17: "seventeen", 18: "eighteen",
   19: "nineteen", 20: "twenty", 30: "thirty", 40: "forty", 50: "fifty",
   60: "sixty", 70: "seventy", 80: "eighty", 90: "ninety", 0: "zero",
   -1: "first", -2: "second", -3: "third", -4: "fourth", -5: "fifth",
   -6: "sixth", -7: "seventh", -8: "eighth", -9: "ninth", -10: "tenth",
   -11: "eleventh", -12: "twelfth", -13: "thirteenth",
   -14: "fourteenth", -15: "fifteenth", -16: "sixteenth",
   -17: "seventeenth", -18: "eighteenth", -19: "nineteenth",
   -20: "twentieth" ]);

/** Converts a number to its written form; for numbers 100 and
 * greater, it uses Arabic notation ("125"). For smaller numbers,
 * it spells it out ("ninety-eight" or "seventeen").
 */
string get_string_from_number( int x ) {
   if( stringp(x) ) return ""+x;
   if( floatp(x) ) x = to_int(floor(x));
   if( x >= 0 && x <= 20 ) return numbers_words_map[x];
   if( x < 0 && x >= -20 ) return numbers_words_map[x];
   if( x >= 100 || x < 0 ) return "" + x;
   if( x % 10 == 0 ) return numbers_words_map[x];
   return numbers_words_map[x / 10 * 10] + "-" + numbers_words_map[x % 10];
}

static mapping adverb_list;

mapping get_adverb_list() {
   if( !adverb_list ) {
      mixed tmp;
      tmp = read_file("/text/adverbs");
      if( tmp ) {
         adverb_list = mkmapping( explode(tmp, "\n") );
         adverb_list -= ([ "" ]);
      }
   }
   if( !adverb_list ) return ([ ]);
   return adverb_list;
}

/** Returns "a" or "an" appropriately for the given word.
 * Bored coders may add exceptiosn for "hour", "honor", etc.
 * \param word The word to return "a" or "an" for
 */
string a_an( string word ) {
   if ( strlen(word) < 1 )
      return "";

   if ( member(({"a", "e", "i", "o", "u"}), word[0]) != -1 )
      return "an";
   else
      return "a";
}
