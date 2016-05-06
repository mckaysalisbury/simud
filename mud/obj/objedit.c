#include <list.h>
#include <const.h>
#include <objedit.h>
#include <wizard.h>
#include <object.h>

/** Acius's Nifty Object Editor.
 *
 * There's a couple things you can do with this object editor:
 * You can make a template, or edit an existing one, or edit
 * an existing object.
 *
 * If you give a valid object name for a .c file, It will clone that
 * file and allow you to edit the variables. This will save a template.
 * It allows you to save back to the original source file as well,
 * but only if there is OBJEDIT delimiting in the create() function
 * (It does not mess up any custom code in the file).
 *
 * If you give a valid object name for a .o file, and the .o file contains
 * a my_file_name field, then it will clone the relevant file, load the
 * .o file, and allow you to edit it that way.
 *
 * If you give a valid object reference for an already existing object,
 * it will simply point to that object and allow you to edit it.
 *
 * To use this object editor, clone it and move it inside the wizard
 * who will use it. Then call the obj_edit(str) function, where str
 * is the string referring to either an object file or actual object.
 *
 * Note: Needs updating!
 */

#define OBJEDIT_PREFIX "/* OBJEDIT { */"
#define OBJEDIT_SUFFIX "/* } OBJEDIT */"

static object obptr;
static string fname, tempname;
static int mode, saved;
string my_file_name; // For determining template sources from the .o file.

init() {
   if(this_player()->query_level() < WIZLEVEL) {
      write("Arrrggg\n");
//      destruct(this_object());
   }
   ::init();
}

obj_query(str) {
   string var, param, left, val;
   int pi;

   if(!str || str == "") {
      write("==> [Format] query var [parameter]\n");
      return;
   }

   if(sscanf(str, "%s %s", var, param) != 2) {
      var = str;
      param = 0;
   }

   if(param && sscanf(param, "%d%s", pi, left) == 2 && left == "")
      val = call_other(obptr, var, pi);
   else val = call_other(obptr, var, param);

   if(!val)
   if(param && sscanf(param, "%d%s", pi, left) == 2 && left == "")
      val = call_other(obptr, "query_"+var, pi);
   else val = call_other(obptr, "query_"+var, param);

   write("==> [Object Editor] Query returned "+as_string(val)+"\n");
}

int obj_save(string str) {
   if(str != "") tempname = this_player()->make_path_absolute(str);
   if(mode == 0) {
      write("==> [Object Editor] Saving object settings\n");
      obptr->save_self();
      saved = 1;
   } else {
      if(!tempname) {
         write("==> [Error] Must give a file name for the first save\n");
         return 0;
      }
      write("==> [Object Editor] Saving template file "+tempname+".o\n");
      obptr->save_template(tempname);
      saved = 1;
   }
   return 1;
}

string *get_detected_modules() {
   string s;
   string *d;

   d = ({});
   foreach( s: inherit_list(obptr) ) {
      s = s[..<3];
      while( sscanf(s, "%~s/%s", s) == 2 );
      if( function_exists(s+"_info", obptr) ) d += ({ s });
   }
   return d;
}

string var_code;

string get_save_source_var(string var) {
   mixed v;

   v = call_other(obptr, "query_" + var);
   v = as_lpc(v);
   if( v != "0" )
      var_code += "   set_"+var+"( " + v + " );\n";
}

string get_save_source_obj(string mod) {
   string varlist;

   if( !stringp(mod) ) return 0;
   varlist = call_other(obptr, mod+"_info");
   if( !stringp(varlist) ) return 0;
   var_code = "";
   call_each(varlist, "get_save_source_var");
   return var_code;
}

string get_save_source() {
   string mod;
   string src;

   src = "";
   foreach( mod: get_detected_modules() )
   if( mod != "saveload" )
      src += get_save_source_obj( mod );

   return src;
}

int obj_savesrc(string str) {
   string mod;
   int rewrite_mode;

   if( str != "" && str ) fname = this_player()->make_path_absolute( str );
   if( !fname ) {
      write("==> [Error] Must give a filename.\n");
      return 0;
   }
   if( fname[<2..<1] != ".c" ) fname += ".c";
   if( !query_write_access(fname, this_player()) ) {
      write("==> [Error] Access denied. Try another file name or location.\n");
      return 0;
   }
   if( file_exists(fname) ) {
      string prefix, suffix;

      mod = read_bytes( fname, 0, file_size(fname) );
      if( sscanf(mod, "%s" + OBJEDIT_PREFIX + "%s" + OBJEDIT_SUFFIX + "%s", prefix, mod, suffix) != 3 ) {
         write("==> [Error] File "+fname+" exists and is missing objedit tags for modification.\n");
         return 0;
      }
      if( !rm(fname) ) {
         write("==> [Error] Could not replace file: Unable to remove old file.\n");
         return 0;
      }
      write_file( fname, prefix + OBJEDIT_PREFIX + "\n" + get_save_source() + OBJEDIT_SUFFIX + suffix );
      write("==> [Object Editor] Modifying source file "+fname+" with updated settings.\n");
      saved = 1;
   }
   else {
      write_file( fname, "inherit \""+load_name(obptr)+"\";\n\nvoid create() {\n" );
      if( function_exists("create", obptr) ) write_file( fname, "   ::create();\n");
      write_file( fname, OBJEDIT_PREFIX + "\n" + get_save_source() + OBJEDIT_SUFFIX + "\n}\n");
      write("==> [Object Editor] Creating definition file "+fname+" (inherits "+load_name(obptr)+")\n");
      saved = 1;
   }
   return 1;
}

obj_browse(str) {
   string infolist;
   string mod;

   if(str == "") {
      write(" __________\n"+
            "| Detected |___\n");
      foreach( mod : get_detected_modules() )
         write("| "+pad(mod, 13, 0)+"|\n");
      write("|______________|\n");
   } else {
      infolist = call_other(obptr, str+"_info");
      if(!infolist) {
         write("==> [Error] That information not available\n");
         return;
      }
      write(" ________________ _______\n"+
            "|    Variable    | Value |" + tab("_",this_player()->query_term_width()-28) + "\n");
      call_each(infolist, "browse_item");
      write("|"+tab("_",this_player()->query_term_width()-3)+"|\n");
   }
}

browse_item(str) {
   string val, s1, s2;

   val = as_string( call_other(obptr, "query_"+str) );
   str = pad(str, 14, 1);
   while(sscanf(val, "%s\n%s", s1, s2) == 2) val = s1 + "/" + s2;
   val = pad(val, this_player()->query_term_width() - 21, 0);
   write("| "+str+" | "+val+"|\n");
}

void obj_call(string str) {
   mixed ret;
   string callstr;

   if( !str ) return;

   callstr = as_string(obptr) + " " + str;

   write("==> Calling " + str + "\n");
   ret = exec_call( callstr );
   if( ret != "0" )
      write("Function returned:\n"+ret+"\n");
}


obj_set(str) {
   if(!str || sscanf(str, "%~s %~s") != 2) {
      write("==> [Format] set var value\n");
      return;
   }
   obj_call("set_"+str);
}

obj_tool(str) {
   string comm, param;

   if(str) {
      if(str == "q" || str == "Q") {
         if(!saved && str == "q" && mode) {
            write("==> [Error] Use Q to quit without saving, or save\n");
            return;
         }
         else {
            drop_interface();
            if(mode) destruct(obptr);
            destruct(this_object());
            return;
         }
      }
      if(str == "?") {
         write(" __________________\n"+
               "| Object Tool Help |_________________\n"+
               "|                                    |\n"+
               "| set - Set value of a var           |\n"+
               "| query - Query value of a var       |\n"+
               "| osave - Saves object to a .o file  |\n"+
               "| csave - Create a .c file           |\n"+
               "| save - Just save it                |\n"+
               "| browse - Analyze object            |\n"+
               "| <func> [params] - Call function    |\n"+
               "| q - Quit object tool               |\n"+
               "|____________________________________|\n");
         return;
      }
      if(sscanf(str, "%s %s", comm, param) != 2) {
         comm = str;
         param = "";
      }
      if(comm == "set") obj_set(param); else
      if(comm == "query") { obj_query(param); return; } else
      if(comm == "osave") { obj_save(param); return; } else
      if(comm == "csave") { obj_savesrc(param); return; } else
      if(comm == "save") { if( tempname ) obj_save(param); else obj_savesrc(param); return; } else
      if(comm == "browse") { obj_browse(param); return; }else
      if(comm && comm != "") obj_call(comm+" "+param);
      saved = 0;
      return;
   }
}

void obj_edit(string str) {
   // First we check if it is a valid .c file
   saved = 1;
   sscanf(str, "%s.c", str);
   sscanf(str, "%s.o", str);
   fname = this_player()->make_path_absolute(str);
   if( file_exists(fname + ".c") ) {
      write("==> [Object Editor] Editing an existing source file\n");
      if( find_object(fname) )
         "/bin/wiz/update"->main( fname ); // Perform a simple update...
      obptr = clone_object(fname);
      obptr->move( this_object() );
      tempname = 0;
      mode = 1;
   }
   else if( file_exists(fname + ".o") ) {
      restore_object(fname);
      if( !my_file_name ) {
         write("==> [Object Editor] Not a valid template\n");
         destruct(this_object());
         return;
      }
      obptr = clone_object(my_file_name);
      obptr->move( this_object() );
      obptr->load_template(fname);
      write("==> [Object Editor] Editing an existing template\n");
      tempname = fname;
      fname = 0;
      mode = 1;
   }
   else {
      write("==> [Object Editor] Editing an existing object\n");
      mode = 0;
      obptr = call_other("obj/wizard","find_thing",str);
      if(!obptr) {
         write("==> [Error] I can't find that\n");
         return;
      }
      fname = obptr->query_object_name();
      if(!fname)
         write("==> [Warning] This object does not have an attached data file\n");
   }
   add_interface("obj_tool", "[Object Editor | ? Help] ");
}

query_distant() { return "an object editor"; }
query_name() { return "objedit"; }
id(str) { return str == "objedit"; }
query_look() { return "Acius's funky object editor. Edits objects."; }
