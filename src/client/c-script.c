/* $Id$ */
/* File: c-script.c */

/* Purpose: scripting in lua */

/*
 * Copyright (c) 2001 Dark God
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */
/* TODO: rename pern_* to tomenet_* :)
 * TODO: make msg_print/msg_admin dummy function and integrate this file
 * with server/script.c
 */

#include "angband.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "tolua.h"

int  tolua_util_open (lua_State *L);
int  tolua_player_open (lua_State *L);
void dump_lua_stack_stdout(int min, int max);
void dump_lua_stack(int min, int max);

/*
 * Lua state
 */
lua_State* L = NULL;

/* PernAngband Lua error message handler */
static int pern_errormessage(lua_State *L)
{
	char buf[200];
	cptr str = luaL_check_string(L, 1);
	int i = 0, j = 0;

	while (str[i])
	{
		if (str[i] != '\n')
		{
			buf[j++] = str[i];
		}
		else
		{
			buf[j] = '\0';
			//msg_broadcast(0, format("\377vLUA: %s", buf));
			//msg_admin("\377vLUA: %s", buf);
			c_msg_format("\377vLUA: %s", buf);
			j = 0;
		}
		i++;
	}
	buf[j] = '\0';
	//msg_broadcast(0, format("\377vLUA: %s", buf));
	//msg_admin("\377vLUA: %s", buf);
	c_msg_format("\377vLUA: %s", buf);
	return (0);
}

static struct luaL_reg pern_iolib[] =
{
        {"_ALERT", pern_errormessage},
};

#define luaL_check_bit(L, n)  ((long)luaL_check_number(L, n))
#define luaL_check_ubit(L, n) ((unsigned long)luaL_check_bit(L, n))


/* DYADIC/MONADIC is commented out in ToME;
 * better avoid to use them maybe */

#define DYADIC(name, op) \
    s32b name(s32b a, s32b b); \
    s32b name(s32b a, s32b b) { \
		return (a op b); \
    }

#define MONADIC(name, op) \
    s32b name(s32b b); \
    s32b name(s32b b) { \
		return (op b); \
    }


DYADIC(intMod,      % )
DYADIC(intAnd,      & )
DYADIC(intOr,       | )
DYADIC(intXor,      ^ )
DYADIC(intShiftl,   <<)
DYADIC(intShiftr,   >>)
MONADIC(intBitNot,  ~ )


/*
 * Monadic bit nagation operation
 * MONADIC(not,     ~)
 */
static int int_not(lua_State* L)
{
	lua_pushnumber(L, ~luaL_check_bit(L, 1));
	return 1;
}


/*
 * Dyadic integer modulus operation
 * DYADIC(mod,      %)
 */
static int int_mod(lua_State* L)
{
	lua_pushnumber(L, luaL_check_bit(L, 1) % luaL_check_bit(L, 2));
    return 1;
}


/*
 * Variable length bitwise AND operation
 * VARIADIC(and,    &)
 */
static int int_and(lua_State *L)
{
	int n = lua_gettop(L), i;
	long w = luaL_check_bit(L, 1);

	for (i = 2; i <= n; i++) w &= luaL_check_bit(L, i);
	lua_pushnumber(L, w);

	return 1;
}


/*
 * Variable length bitwise OR operation
 * VARIADIC(or,     |)
 */
static int int_or(lua_State *L)
{
	int n = lua_gettop(L), i;
	long w = luaL_check_bit(L, 1);

	for (i = 2; i <= n; i++) w |= luaL_check_bit(L, i);
    lua_pushnumber(L, w);

    return 1;
}


/*
 * Variable length bitwise XOR operation
 * VARIADIC(xor,    ^)
 */
static int int_xor(lua_State *L)
{
	int n = lua_gettop(L), i;
	long w = luaL_check_bit(L, 1);

	for (i = 2; i <= n; i++) w ^= luaL_check_bit(L, i);
    lua_pushnumber(L, w);

    return 1;
}


/*
 * Binary left shift operation
 * TDYADIC(lshift,  <<, , u)
 */
static int int_lshift(lua_State* L)
{
	lua_pushnumber(L, luaL_check_bit(L, 1) << luaL_check_ubit(L, 2));
    return 1;
}

/*
 * Binary logical right shift operation
 * TDYADIC(rshift,  >>, u, u)
 */
static int int_rshift(lua_State* L)
{
	lua_pushnumber(L, luaL_check_ubit(L, 1) >> luaL_check_ubit(L, 2));
	return 1;
}

/*
 * Binary arithmetic right shift operation
 * TDYADIC(arshift, >>, , u)
 */
static int int_arshift(lua_State* L)
{
	lua_pushnumber(L, luaL_check_bit(L, 1) >> luaL_check_ubit(L, 2));
	return 1;
}


static const struct luaL_reg bitlib[] =
{
        {"bnot",    int_not},
        {"imod",    int_mod},  /* "mod" already in Lua math library */
        {"band",    int_and},
        {"bor",     int_or},
        {"bxor",    int_xor},
        {"lshift",  int_lshift},
        {"rshift",  int_rshift},
        {"arshift", int_arshift},
};


/* Initialize lua scripting */
bool init_lua_done = FALSE;
void init_lua()
{
        if (init_lua_done) return;

	/* Start the interpreter with default stack size */
	L = lua_open(0);

	/* Register the Lua base libraries */
	lua_baselibopen(L);
	lua_strlibopen(L);
	lua_iolibopen(L);
	lua_dblibopen(L);

	/* Register pern lua debug library */
	luaL_openl(L, pern_iolib);

	/* Register the bitlib */
	luaL_openl(L, bitlib);

	/* Register the TomeNET main APIs */
	tolua_util_open(L);
	tolua_player_open(L);
        tolua_spells_open(L);

        /* Load the xml module */
	pern_dofile(0, "xml.lua");
	pern_dofile(0, "meta.lua");

        init_lua_done = TRUE;
}

void open_lua()
{
	int i, max;

        if (!init_lua_done) init_lua();

	/* Load the first lua file */
	pern_dofile(0, "c-init.lua");

	/* Finish up schools */
	max = exec_lua(0, "return __schools_num");
	init_schools(max);
	for (i = 0; i < max; i++)
	{
		exec_lua(0, format("finish_school(%d)", i));
	}

	/* Finish up the spells */
	max = exec_lua(0, "return __tmp_spells_num");
	init_spells(max);
	for (i = 0; i < max; i++)
	{
		exec_lua(0, format("finish_spell(%d)", i));
	}
}

void dump_lua_stack(int min, int max)
{
        int i;

        c_msg_print("\377ylua_stack:");
        for (i = min; i <= max; i++)
        {
                if (lua_isnumber(L, i)) c_msg_format("\377y%d [n] = %d", i, tolua_getnumber(L, i, 0));
                else if (lua_isstring(L, i)) c_msg_format("\377y%d [s] = '%s'", i, tolua_getstring(L, i, 0));
        }
        c_msg_print("\377yEND lua_stack");
}

void dump_lua_stack_stdout(int min, int max)
{
        int i;

        printf("ylua_stack:\n");
        for (i = min; i <= max; i++)
        {
                if (lua_isnumber(L, i)) printf("%d [n] = %ld\n", i, tolua_getnumber(L, i, 0));
                else if (lua_isstring(L, i)) printf("%d [s] = '%s'\n", i, tolua_getstring(L, i, 0));
        }
        printf("END lua_stack\n");
}

bool pern_dofile(int Ind, char *file)
{
	char buf[MAX_PATH_LENGTH];
	int error;
        int oldtop = lua_gettop(L);
        char ind[80];

	/* Build the filename */
        path_build(buf, MAX_PATH_LENGTH, ANGBAND_DIR_SCPT, file);

        sprintf(ind, "Ind = %d; player = Players_real[2]", 1);
        lua_dostring(L, ind);
        lua_settop(L, oldtop);

        error = lua_dofile(L, buf);
        lua_settop(L, oldtop);

        return (error?TRUE:FALSE);
}

int exec_lua(int Ind, char *file)
{
	int oldtop = lua_gettop(L);
        int res;
        char ind[80];

        sprintf(ind, "Ind = %d; player = Players_real[2]", 1);
        lua_dostring(L, ind);
        lua_settop(L, oldtop);

        if (!lua_dostring(L, file))
        {
                int size = lua_gettop(L) - oldtop;
                res = tolua_getnumber(L, -size, 0);
        }
	else
                res = 0;

        lua_settop(L, oldtop);
	return (res);
}

cptr string_exec_lua(int Ind, char *file)
{
	int oldtop = lua_gettop(L);
	cptr res;
        char ind[80];

        sprintf(ind, "Ind = %d; player = Players_real[2]", 1);
        lua_dostring(L, ind);
        lua_settop(L, oldtop);

	if (!lua_dostring(L, file))
        {
                int size = lua_gettop(L) - oldtop;
                res = tolua_getstring(L, -size, "");
        }
	else
		res = "";
        lua_settop(L, oldtop);
	return (res);
}

static FILE *lua_file;
void master_script_begin(char *name, char mode)
{
	char buf[MAX_PATH_LENGTH];

	/* Build the filename */
        path_build(buf, MAX_PATH_LENGTH, ANGBAND_DIR_SCPT, name);

        switch (mode)
        {
        case MASTER_SCRIPTB_W:
                lua_file = my_fopen(buf, "w");
                break;
        case MASTER_SCRIPTB_A:
                lua_file = my_fopen(buf, "a");
                break;
        }
        if (!lua_file)
                plog(format("ERROR: creating lua file %s in mode %c", buf, mode));
        else
                plog(format("Creating lua file %s in mode %c", buf, mode));

}

void master_script_end()
{
        my_fclose(lua_file);
}

void master_script_line(char *buf)
{
        fprintf(lua_file, "%s\n", buf);
}

void master_script_exec(int Ind, char *buf)
{
        exec_lua(Ind, buf);
}

void cat_script(char *name)
{
        char buf[1025];
        FILE *fff;

        /* Build the filename */
        path_build(buf, 1024, ANGBAND_DIR_SCPT, name);
        fff = my_fopen(buf, "r");
        if (fff == NULL) return;

        /* Process the file */
        while (0 == my_fgets(fff, buf, 1024))
        {
                c_msg_print(buf);
        }
}

bool call_lua(int Ind, cptr function, cptr args, cptr ret, ...)
{
        int i = 0, nb = 0, nbr = 0;
        int oldtop = lua_gettop(L), size;
	va_list ap;

        char ind[80];
        sprintf(ind, "Ind = %d; player = Players_real[2]", 1);
        lua_dostring(L, ind);
        lua_settop(L, oldtop);

        va_start(ap, ret);

        /* Push the function */
        lua_getglobal(L, function);

        /* Push and count the arguments */
        while (args[i])
        {
                switch (args[i++])
                {
                case 'd':
                case 'l':
                        tolua_pushnumber(L, va_arg(ap, s32b));
                        nb++;
                        break;
                case 's':
                        tolua_pushstring(L, va_arg(ap, char*));
                        nb++;
                        break;
                case 'O':
                        tolua_pushusertype(L, (void*)va_arg(ap, object_type*), tolua_tag(L, "object_type"));
                        nb++;
                        break;
                case '(':
                case ')':
                case ',':
                        break;
                }
        }

        /* Count returns */
        nbr = strlen(ret);
 
        /* Call the function */
        if (lua_call(L, nb, nbr))
        {
                c_msg_format("#yERROR in lua_call while calling '%s' from call_lua. Things should start breaking up from now on!", function);
                return FALSE;
        }

        /* Number of returned values, SHOULD be the same as nbr, but I'm paranoid */
        size = lua_gettop(L) - oldtop;

        /* Get the returns */
        for (i = 0; ret[i]; i++)
        {
                switch (ret[i])
                {
                case 'd':
                case 'l':
                        {
                                s32b *tmp = va_arg(ap, s32b*);

                                if (lua_isnumber(L, (-size) + i)) *tmp = tolua_getnumber(L, (-size) + i, 0);
                                else *tmp = 0;
                                break;
                        }

                case 's':
                        {
                                cptr *tmp = va_arg(ap, cptr*);

                                if (lua_isstring(L, (-size) + i)) *tmp = tolua_getstring(L, (-size) + i, "");
                                else *tmp = NULL;
                                break;
                        }

                case 'O':
                        {
                                object_type **tmp = va_arg(ap, object_type**);

                                if (tolua_istype(L, (-size) + i, tolua_tag(L, "object_type"), 0))
                                        *tmp = (object_type*)tolua_getuserdata(L, (-size) + i, NULL);
                                else
                                        *tmp = NULL;
                                break;
                        }

                default:
                        c_msg_format("#yERROR in lua_call while calling '%s' from call_lua: Unkown return type '%c'", function, ret[i]);
                        return FALSE;
                }
        }
        lua_settop(L, oldtop);

        va_end(ap);

        return TRUE;
}
