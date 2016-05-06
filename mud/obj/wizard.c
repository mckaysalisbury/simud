#include <const.h>

#define EXEC_OBJ "~/call_exec"
#define EXEC_MACROS "#define COM(x) command(x, this_player())\n" + \
                    "#define TP this_player()\n" + \
                    "#define TO this_object()\n" + \
                    "#define FO(x) find_object(x)\n" + \
                    "#define ON(x) object_name(x)\n" + \
                    "#define FP(x) find_player(x)\n" + \
                    "#define FL(x) find_living(x)\n" + \
                    "#define HERE environment(this_player())\n" + \
                    "#define ENV(x) environment(x)\n"

#define HEADER "/* This is a MUDlib generated file. Do not edit! */\n" + \
               "#include <wizard.h>\n" + \
               "#include <sys/regexp.h>\n"


/** A simple but critical object; this object is used by call, lpc, query,
 * and a few other important bits of code that need to execute LPC code
 * on the fly.
 */

/** Converts a reference into an object pointer. You should only
 * pass in one parameter; the second \a rcheck parameter is used
 * internally.
 *
 * \return The object referenced by \a str, or 0 on failure.
 */
varargs object find_thing(string str, status rcheck) {
   object ob;
   string a, b;

   if(!str) return 0;
   if( this_player() ) {
      ob = present(str, this_player());
      if(environment(this_player())) {  /* Believe it or not, this was necessary. */
         if(!ob) ob = present(str, environment(this_player()));
         if(!ob && (environment(this_player())->id(str) == environment(this_player()) || str == "env"))
            ob = environment(this_player());
      }
   }
   if(!ob) {
      ob = find_player(str);
   }
   if(!ob) {
      ob = find_living(str);
   }
   if(!ob) {
      ob = find_object(str);
   }
   if(!ob) {
      ob = find_objectref(str);
   }
   if(!ob && sscanf(str, "%s %s",a,b)!=2 && file_exists(str+".c") ) {
      call_other(str, "this_function_does_not_exist\n");
      ob = find_object(str);
   }
   if(!ob && str == "me") ob = this_player();
   if(!ob) ob = find_room( str, this_player() );
   if(!ob) ob = single_present( str, this_player() );
   if(!ob) ob = single_present( str, environment(this_player()) );
   if(!rcheck && !ob && this_player() )
      ob = find_thing(this_player()->make_path_absolute(str), 1);
   return ob;
}

/** A highly unusual function that lets you generate and execute a snippet of code
 * that will execute. Just pass in the snippet. Warning: This can't handle recursive
 * calls; the old call will be trashed by the new.
 *
 * \return The return value of the call, converted to a string.
 */
string exec_call(string code) {
   string buf;
   string obj, func, func_name, err;
   mixed param, ret;
   string fname;

   if( !this_player() )
      raise_error("exec_call must have a valid this_player() using it");

   fname = this_player()->make_path_absolute( EXEC_OBJ );

   if( ret = find_object(fname) ) destruct(ret);
   if( file_exists(fname + ".c") && !rm( fname + ".c" ) )
      return "Unable to clear out the old "+fname+" to execute the call (uid="+getuid(this_object())+").";

   buf = HEADER + EXEC_MACROS + "\ndo_call() {\n";

   if( sscanf(code, "%s->%s", obj, func) == 2 ) {
      func_name = func;
      if( sscanf(func, "%~s(%~s)") != 2 ) {
         code = func;
         if( sscanf( code, "%s %d", func, param) == 2)
            func = (func_name = func) + "( " + param + " );";
         else
         if( sscanf( code, "%s %s", func, param) == 2)
            func = (func_name = func) + "( \"" + param + "\" );";
         else
            func = (func_name = code) + "();";
      }
      else sscanf( func_name, "%s(%~s)", func_name );
   }
   else if( sscanf(code, "%s %s(%s)%~s", obj, func, param) == 4 && member(func, ' ') < 0 )
      func = (func_name = func) + "( " + param +" );";
   else if( sscanf(code, "%s %s %d", obj, func, param) == 3 )
      func = (func_name = func) + "( " + param + " );";
   else if( sscanf(code, "%s %s %s", obj, func, param) == 3 )
      func = (func_name = func) + "( \"" + param + "\" );";
   else if( sscanf(code, "%s %s", obj, func) == 2 )
      func = (func_name = func) + "();";
   else raise_error("Ill formatted exec_call");

   if( !find_thing(obj) ) return "No such object: "+obj;
   if( !function_exists(func_name, find_thing(obj)) && !function_exists("catch_call", find_thing(obj)) )
      return "No such function: "+obj+"->"+func_name+"()";

//   if( sscanf(func, "%s(%s)%s", ret, ret, ret) != 3 ) func += "();";

   if( func[<1] != ';' ) func += ";";
   buf += "   object ob;\n";
   buf += "   ob = find_thing(\""+obj+"\");\n";
   buf += "   if( !ob ) return \"Could not find object "+obj+"!\";\n";
   buf += "   return ob->"+func+"\n}\n";

   write_file( fname + ".c", buf );
   err = catch(ret = load_object( fname ));
   if( err ) {
      msg("==> [Call Error] "+err);
      return "<Invalid>";
   }

   seteuid( getuid(this_player()) );
   export_uid( ret );
   seteuid( getuid(this_object()) );
   err = catch(ret = fname->do_call());
   if( err ) {
      msg("==> [Call Runtime Error] "+err);
      return "<Invalid>";
   }

   return as_lpc(ret);
}

/** A more generic function than exec_call, this one actually lets you
 * execute any piece of code (more than just a simple function call).
 * It automatically wraps the snippet in a function.
 *
 * \param code The code to execute.
 */
string exec_code(string code) {
   string buf, err;
   mixed ret;
   string fname;

   if( !this_player() )
      raise_error("exec_code must have a valid this_player() using it");

   fname = this_player()->make_path_absolute( EXEC_OBJ );

   if( ret = find_object(fname) ) destruct(ret);
   if( file_exists(fname + ".c") &&  !rm( fname + ".c" ) )
      return "Unable to remove file "+fname+" to execute call.";
   buf = HEADER +
         EXEC_MACROS +"\ndo_exec() {\n";

   buf += code + "\n}\n";


   write_file( fname + ".c", buf );
   err = catch(ret = load_object( fname ));
   if( err ) {
      msg("==> [Exec Error] "+err);
      return "<Invalid>";
   }

   seteuid( getuid(this_player()) );
   export_uid( ret );
   seteuid( getuid(this_object()) );
   ret = 0;
   err = catch(ret = fname->do_exec());
   if( err ) {
      msg("==> [Call Runtime Error] "+err);
      return "<Invalid>";
   }

   return as_lpc(ret);
}
