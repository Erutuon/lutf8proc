#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "lua.h"
#include "lauxlib.h"
#include "utf8proc.h"

// Use bit options in map and map_custom.
// #define USE_BIT_OPTIONS

#define LUA_SETTYPE(L, k, v, type) \
	lua_push##type(L, v), lua_setfield(L, -2, k)

#define TO_UPPER(ch) \
	(('a' <= (ch) && (ch) <= 'z') ? (ch) - 0x20 : (ch))

#define CODEPOINT_IN_RANGE(cp) (0 <= (cp) && (cp) <= 0x10FFFF)

#define ARRAY_LENGTH(arr) (sizeof (arr) / sizeof (arr)[0])

typedef struct category_desc {
	const char code[3];
	const char * const description;
} category_desc;

// PropertyValueAliases.txt
// The categories must be in the same order as in the enum in utf8proc.h!
static const category_desc category_names[] = {
	{ "Cn", "Other, not assigned" },
	{ "Lu", "Letter, uppercase" },
	{ "Ll", "Letter, lowercase" },
	{ "Lt", "Letter, titlecase" },
	{ "Lm", "Letter, modifier" },
	{ "Lo", "Letter, other" },
	{ "Mn", "Mark, nonspacing" },
	{ "Mc", "Mark, spacing combining" },
	{ "Me", "Mark, enclosing" },
	{ "Nd", "Number, decimal digit" },
	{ "Nl", "Number, letter" },
	{ "No", "Number, other" },
	{ "Pc", "Punctuation, connector" },
	{ "Pd", "Punctuation, dash" },
	{ "Ps", "Punctuation, open" },
	{ "Pe", "Punctuation, close" },
	{ "Pi", "Punctuation, initial quote" },
	{ "Pf", "Punctuation, final quote" },
	{ "Po", "Punctuation, other" },
	{ "Sm", "Symbol, math" },
	{ "Sc", "Symbol, currency" },
	{ "Sk", "Symbol, modifier" },
	{ "So", "Symbol, other" },
	{ "Zs", "Separator, space" },
	{ "Zl", "Separator, line" },
	{ "Zp", "Separator, paragraph" },
	{ "Cc", "Other, control" },
	{ "Cf", "Other, format" },
	{ "Cs", "Other, surrogate" },
	{ "Co", "Other, private use" },
};

#define CATEGORY_COUNT  (ARRAY_LENGTH(category_names))

#ifdef USE_BIT_OPTIONS
typedef struct option {
	const char * const name;
	const int value;
} option;

#ifdef USE_UPPER_OPTION
#define OPT(lower_option, upper_option) \
	{ #upper_option, UTF8PROC_##upper_option }
#else
#define OPT(lower_option, upper_option) \
	{ #lower_option, UTF8PROC_##upper_option }
#endif // USE_UPPER_OPTION

static const option option_list[] = {
	OPT(nullterm,  NULLTERM ),
	OPT(stable,    STABLE   ),
	OPT(compat,    COMPAT   ),
	OPT(compose,   COMPOSE  ),
	OPT(decompose, DECOMPOSE),
	OPT(ignore,    IGNORE   ),
	OPT(rejectna,  REJECTNA ),
	OPT(nlf2lf,    NLF2LF   ), // Order required by show_options.
	OPT(nlf2ls,    NLF2LS   ),
	OPT(nlf2ps,    NLF2PS   ),
	OPT(stripcc,   STRIPCC  ),
	OPT(casefold,  CASEFOLD ),
	OPT(charbound, CHARBOUND),
	OPT(lump,      LUMP     ),
	OPT(stripmark, STRIPMARK),
};

#undef DEFINE_OPTION

#define OPTION_COUNT (ARRAY_LENGTH(option_list))
#endif // USE_BIT_OPTIONS

#define MAX_OPTION_BIT 13
#define MASK_N(n) (~((1 << ((n) + 1)) - 1))
// Only the 13 lowest bits are used by options.
#define CHECK_OPTIONS(o) (((o) & MASK_N(MAX_OPTION_BIT)) == 0)
// undefined at bottom

#ifndef USE_BIT_OPTIONS

#ifdef WINDOWS
#define strcasecmp _stricmp
#endif // WINDOWS

static int lua_case_insensitive_check_option (lua_State * L, int arg,
											  const char *def,
											  const char * const options[]) {
	const char *name = (def)
		? luaL_optstring(L, arg, def) : luaL_checkstring(L, arg);
	int i;
	for (i = 0; options[i]; i++)
		if (strcasecmp(options[i], name) == 0)
			return i;
	return luaL_argerror(L, arg,
						 lua_pushfstring(L, "invalid option '%s'", name));
}
#endif // not defined USE_BIT_OPTIONS

/*
utf8proc_ssize_t utf8proc_iterate
	(const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_int32_t *codepoint_ref)
*/

/*
 *  If value at index is integer, return it; if string, return first codepoint.
 *  Error if beginning of string is invalid UTF-8.
 */
static utf8proc_int32_t get_codepoint (lua_State * L, int idx, int throw_err) {
	utf8proc_int32_t codepoint;
	switch (lua_type(L, idx)) {
		case LUA_TSTRING: {
			size_t str_len;
			const char * str = luaL_checklstring(L, idx, &str_len);
			if (str_len == 0) { /* Empty string is valid UTF-8. */
				return 1;
			}
			/*	Read single codepoint. Negative value is returned and written
				to 'codepoint' variable in case of error. */
			utf8proc_ssize_t err_code =
				utf8proc_iterate((const utf8proc_uint8_t *) str,
								 (utf8proc_ssize_t) -1, &codepoint);
			if (err_code < 0 && throw_err)
				return luaL_error(L, utf8proc_errmsg(err_code));
			else
				break;
		}
		case LUA_TNUMBER:
			codepoint = (utf8proc_int32_t) luaL_checkinteger(L, 1);
			if (!CODEPOINT_IN_RANGE(codepoint)) {
				if (throw_err)
					return luaL_argerror(L, idx, "value out of range");
				else
					codepoint = -1;
			}
			break;
		default: {
			luaL_argerror(L, idx,
				lua_pushfstring(L, "string or number expected, got %s",
					luaL_typename(L, idx)));
		}
	}
	return codepoint;
}

/*
const char *utf8proc_category_string(utf8proc_int32_t codepoint);
*/

/* Return the category name for a codepoint, or for the first UTF-8 character in a string. */
static int get_category (lua_State * L) {
	utf8proc_int32_t codepoint = get_codepoint(L, 1, 1);
	lua_pushstring(L, utf8proc_category_string(codepoint));
	return 1;
}

/*
utf8proc_category_t utf8proc_category(utf8proc_int32_t codepoint);
*/
static int get_category_desc (lua_State * L) {
	utf8proc_int32_t codepoint = get_codepoint(L, 1, 1);
	lua_pushstring(L, category_names[utf8proc_category(codepoint)].description);
	return 1;
}

/*
typedef utf8proc_int16_t utf8proc_propval_t;

typedef struct utf8proc_property_struct {
		// Unicode category.
		 // see utf8proc_category_t
	utf8proc_propval_t category;
	utf8proc_propval_t combining_class;
		// Bidirectional class.
		// see utf8proc_bidi_class_t.
	utf8proc_propval_t bidi_class;
		// Decomposition type.
		// see utf8proc_decomp_type_t.
	utf8proc_propval_t decomp_type;
	utf8proc_uint16_t decomp_seqindex;
	utf8proc_uint16_t casefold_seqindex;
	utf8proc_uint16_t uppercase_seqindex;
	utf8proc_uint16_t lowercase_seqindex;
	utf8proc_uint16_t titlecase_seqindex;
	utf8proc_uint16_t comb_index;
	unsigned bidi_mirrored:1;
	unsigned comp_exclusion:1;
		//Can this codepoint be ignored?
		// Used by @ref utf8proc_decompose_char when @ref UTF8PROC_IGNORE is
		// passed as an option.
	unsigned ignorable:1;
	unsigned control_boundary:1;
		// The width of the codepoint.
	unsigned charwidth:2;
	unsigned pad:2;
		// Boundclass.
		// see utf8proc_boundclass_t.
	unsigned boundclass:8;
} utf8proc_property_t;
*/

/* const utf8proc_property_t *utf8proc_get_property(utf8proc_int32_t codepoint) */

static int get_properties(lua_State * L) {
	utf8proc_int32_t codepoint = get_codepoint(L, 1, 1);
	const utf8proc_property_t * props = utf8proc_get_property(codepoint);
	
	lua_newtable(L);
#define SET_PROP(prop, type) LUA_SETTYPE(L, #prop, props->prop, type)
	SET_PROP(bidi_mirrored,    boolean);
	SET_PROP(control_boundary, boolean);
	SET_PROP(comp_exclusion,   boolean);
	SET_PROP(category,         integer);
	SET_PROP(combining_class,  integer);
	SET_PROP(boundclass,	   integer);
#undef SET_PROP
	
	return 1;
}

typedef enum {
	NFD,
	NFC,
	NFKD,
	NFKC
} normalization_t;

#define NORMALIZATION_CASE(normalization, str, new_str) \
	case normalization:                                 \
		new_str = (char *) utf8proc_##normalization(str);           \
		break
/* utf8proc_uint8_t *utf8proc_NFD(const utf8proc_uint8_t *str); */
static int normalize (lua_State * L, int idx, normalization_t mode) {
	size_t str_len;
	const utf8proc_uint8_t * str =
		(const utf8proc_uint8_t *) luaL_checklstring(L, idx, &str_len);
	if (str_len > 0) {
		char * new_str;
		switch (mode) {
			NORMALIZATION_CASE(NFD,  str, new_str);
			NORMALIZATION_CASE(NFC,  str, new_str);
			NORMALIZATION_CASE(NFKD, str, new_str);
			NORMALIZATION_CASE(NFKC, str, new_str);
			default:
				luaL_error(L, "invalid mode for normalization");
		}
		if (new_str == NULL)
			luaL_error(L, "invalid UTF-8 sequence");
		lua_pushstring(L, new_str);
		free(new_str);
	}
	return 1;
}
#undef NORMALIZATION_CASE

/* Decompose a string (convert to NFD). */
static int decompose (lua_State * L) {
	return normalize(L, 1, NFD);
}

/* Compose a string (convert to NFC). */
static int compose (lua_State * L) {
	return normalize(L, 1, NFC);
}

static int compat_decompose (lua_State * L) {
	return normalize(L, 1, NFKD);
}

static int compat_compose (lua_State * L) {
	return normalize(L, 1, NFKC);
}

static int lua_normalize (lua_State * L) {
	size_t mode_len;
	const char * mode = luaL_checklstring(L, 2, &mode_len);
	
	if ((mode_len == 3 || (mode_len == 4 && TO_UPPER(mode[2]) == 'K'))
		 && TO_UPPER(mode[0]) == 'N' && TO_UPPER(mode[1]) == 'F') {
		switch (TO_UPPER(mode[mode_len - 1])) {
			case 'D':
				return (mode_len == 4)
					? normalize(L, 1, NFKD) : normalize(L, 1, NFD);
			case 'C':
				return (mode_len == 4)
					? normalize(L, 1, NFKC) : normalize(L, 1, NFC);
		}
	}
	return luaL_error(L, "normalization form '%s' not recognized; "
		"valid forms 'NFD', 'NFC', 'NFKD', 'NFKC'",
		mode);
}

/* Check validity of single codepoint (or first UTF-8 character in string). */
static int valid_codepoint (lua_State * L) {
	utf8proc_int32_t codepoint = get_codepoint(L, 1, 0);
	lua_pushboolean(L, codepoint >= 0 ? utf8proc_codepoint_valid(codepoint) : 0);
	return 1;
}

/*
utf8proc_ssize_t utf8proc_iterate
	(const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_int32_t *codepoint_ref)
*/
/* utf8proc_int32_t utf8proc_tolower(utf8proc_int32_t c); */
/* utf8proc_int32_t utf8proc_toupper(utf8proc_int32_t c); */
/* utf8proc_ssize_t utf8proc_encode_char(utf8proc_int32_t uc, utf8proc_uint8_t *dst) */
// Use utf8proc_map_custom? Probably less efficient.
static int change_case (lua_State * L,
						utf8proc_int32_t (* change_codepoint) (utf8proc_int32_t)) {
	size_t str_len;
	const char * str = luaL_checklstring(L, 1, &str_len);
	utf8proc_uint8_t * result;
	utf8proc_ssize_t new_len;
	
	new_len = utf8proc_map_custom((const utf8proc_uint8_t *) str,
		(utf8proc_ssize_t) str_len, &result, (utf8proc_option_t) 0,
		(utf8proc_custom_func) change_codepoint, NULL);
	
	if (new_len < 0)
		return luaL_error(L, utf8proc_errmsg(new_len));
	
	lua_pushlstring(L, (const char *) result, (size_t) new_len);
	
	free(result);
	
	return 1;
}

static int to_lower (lua_State * L) {
	return change_case(L, utf8proc_tolower);
}

static int to_upper (lua_State * L) {
	return change_case(L, utf8proc_toupper);
}

static int to_title (lua_State * L) {
	return change_case(L, utf8proc_totitle);
}

static utf8proc_option_t get_options (lua_State * L, int arg, bool no_option_error) {
	utf8proc_option_t options;
#ifdef USE_BIT_OPTIONS
	options = (utf8proc_option_t) luaL_checkinteger(L, arg); /* silently convert */
#else
	static const char * option_strings[] = {
		"nullterm",
		"stable",
		"compat",
		"compose",
		"decompose",
		"ignore",
		"rejectna",
		"nlf2ls",
		"nlf2ps",
		"stripcc",
		"casefold",
		"charbound",
		"lump",
		"stripmark",
		"nlf2lf",
		NULL
	};
	// Make case-insensitive?
	for (int top = lua_gettop(L); arg <= top; ++arg) {
		utf8proc_option_t option = lua_case_insensitive_check_option(L, arg, NULL, option_strings);
		if (option == ARRAY_LENGTH(option_strings) - 1)
			option = UTF8PROC_NLF2LF;
		else
			option = 1 << option;
		
		if ((option & options)
				|| ((option & UTF8PROC_NLF2LF) && (options & UTF8PROC_NLF2LF)))
			luaL_error(L, "conflicting options or option already set");
		
		options |= option;
	}
#endif // USE_BIT_OPTIONS
	if (no_option_error && options == 0)
		return luaL_error(L, "no options were selected");
	else if (!CHECK_OPTIONS(options))
		return luaL_error(L, "invalid options");
	return options;
}

/*
utf8proc_ssize_t utf8proc_map(
	const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_uint8_t **dstptr, utf8proc_option_t options
*/
// Convert to use luaL_checkoption.
static int lua_map (lua_State * L) {
	utf8proc_option_t options;
	size_t str_len;
	const char * str = luaL_checklstring(L, 1, &str_len);
	if (str_len == 0) { /* Return empty string. */
		lua_settop(L, 1);
		return 1;
	}
	options = get_options(L, 2, true);
	utf8proc_uint8_t * result;
	// Problematic if string is longer than maximum utf8proc_ssize_t.
	utf8proc_ssize_t new_len =
		utf8proc_map((const utf8proc_uint8_t *) str, (utf8proc_ssize_t) str_len,
						 &result, (utf8proc_option_t) options);
	if (new_len < 0) /* error code */
		return luaL_error(L, utf8proc_errmsg(new_len));
	
	lua_pushlstring(L, (const char *) result, (size_t) new_len);
	free(result);
	return 1;
}

// typedef utf8proc_int32_t (*utf8proc_custom_func)(utf8proc_int32_t codepoint, void *data);
// Call function on top of stack on a codepoint.
// stack[3] must be function.
static utf8proc_int32_t custom_func (utf8proc_int32_t codepoint, void * data) {
	lua_State * L = (lua_State *) data;
#ifdef USE_BIT_OPTIONS
	lua_pushvalue(L, 3); // Push function.
#else
	lua_pushvalue(L, 2); // Push function.
#endif // USE_BIT_OPTIONS
	lua_pushinteger(L, (lua_Integer) codepoint); // Push codepoint.
	lua_call(L, 1, 1); // func(codepoint)
	utf8proc_int32_t result;
	switch (lua_type(L, 4)) {
		case LUA_TNUMBER: {
			int isnum;
			result = (utf8proc_int32_t) lua_tointegerx(L, 4, &isnum);
			if (!isnum) {
				luaL_error(L, "wrong return value (number has no integer representation)");
			}
			break;
		}
		case LUA_TNIL: case LUA_TNONE: /* Leave codepoint unchanged. */
			result = codepoint; break;
		default:
			luaL_error(L, "wrong return value (integer expected, got %s)", luaL_typename(L, 4));
	}
	lua_settop(L, 3);
	return result;
}

/*
utf8proc_ssize_t utf8proc_map_custom(
	const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_uint8_t **dstptr, utf8proc_option_t options,
	utf8proc_custom_func custom_func, void *custom_data
);
*/
// Args: string, options (integer), function (receives codepoint, returns codepoint).
static int lua_map_custom (lua_State * L) {
	utf8proc_option_t options;
	size_t str_len;
	const char * str = luaL_checklstring(L, 1, &str_len);
	if (str_len == 0) { /* Return empty string. */
		lua_settop(L, 1);
		return 1;
	}
#ifdef USE_BIT_OPTIONS
	options = get_options(L, 2, false);
	luaL_checktype(L, 3, LUA_TFUNCTION);
	lua_settop(L, 3); // Ensure function is on top of stack.
#else
	luaL_checktype(L, 2, LUA_TFUNCTION);
	options = get_options(L, 3, false); // At the moment, options use varargs.
	lua_settop(L, 2);
#endif // USE_BIT_OPTIONS
	utf8proc_uint8_t * result;
	// Problematic if string is longer than maximum utf8proc_ssize_t.
	utf8proc_ssize_t new_len =
		utf8proc_map_custom((const utf8proc_uint8_t *) str,
							(utf8proc_ssize_t) str_len, &result,
							(utf8proc_option_t) options,
							(utf8proc_custom_func) custom_func, (void *) L);
	if (new_len < 0) /* error code */
		return luaL_error(L, utf8proc_errmsg(new_len));
	
	lua_pushlstring(L, (const char *) result, (size_t) new_len);
	free(result);
	return 1;
}

#ifdef USE_BIT_OPTIONS
/* Return the names of the options set in the integer argument. */
static int interpret_options (lua_State * L) {
	lua_Integer options = luaL_checkinteger(L, 1);
	if (options == 0)
		return 0;
	const option * o;
	int i = 0,
		n = 0; // number of options
	while (options && i < OPTION_COUNT) {
		o = &option_list[i++];
		if (options & o->value) {
			lua_pushstring(L, o->name), ++n;
			options &= ~o->value; // Remove option bits.
			// Skip UTF8PROC_NLF2LS and UTF8PROC_NLF2PS.
			if (o->value == UTF8PROC_NLF2LF)
				i += 2;
		}
	}
	if (options)
		return luaL_error(L, "options invalid");
	return n;
}
#endif // USE_BIT_OPTIONS

static int push_category_map (lua_State * L) {
	lua_createtable(L, CATEGORY_COUNT, CATEGORY_COUNT * 2);
	const category_desc * cat;
	for (int i = 0; i < CATEGORY_COUNT; ++i) {
		cat = &category_names[i];
		LUA_SETTYPE(L, cat->code, cat->description, string); /* map[code] = description */
		LUA_SETTYPE(L, cat->description, cat->code, string); /* map[description] = code */
		lua_pushstring(L, cat->code);
		lua_seti(L, -2, i + 1); /* map[index] = code */
	}
	return 1;
}

#ifdef USE_BIT_OPTIONS
static int push_options (lua_State * L) {
	luaL_newlibtable(L, option_list);
	for (int i = 0; i < OPTION_COUNT; ++i) {
		lua_pushinteger(L, option_list[i].value);
		lua_setfield(L, -2, option_list[i].name);
	}
	return 1;
}
#endif // USE_BIT_OPTIONS

static void push_utf8class (lua_State * L) {
	lua_createtable(L, 255, 1);
	for (lua_Integer i = 0; i < ARRAY_LENGTH(utf8proc_utf8class); ++i) {
		lua_pushinteger(L, utf8proc_utf8class[i]);
		lua_rawseti(L, -2, i);
	}
}

static luaL_Reg funcs[] = {
	{ "cat",               get_category      },
	{ "catdesc",           get_category_desc },
	{ "decomp",            decompose         },
	{ "comp",              compose           },
	{ "cdecomp",           compat_decompose  },
	{ "ccomp",             compat_compose    },
	{ "props",             get_properties    },
	{ "valid",             valid_codepoint   },
	{ "normalize",         lua_normalize     },
	{ "lower",             to_lower          },
	{ "upper",             to_upper          },
	{ "title",             to_title          },
	{ "map",               lua_map           },
	{ "map_custom",        lua_map_custom    },
#ifdef USE_BIT_OPTIONS
	{ "interpret_options", interpret_options },
#endif // USE_BIT_OPTIONS
	{ NULL,                NULL              }
};

int luaopen_lutf8proc (lua_State * L) {
	luaL_newlib(L, funcs);
	lua_pushstring(L, utf8proc_version()), lua_setfield(L, -2, "_VERSION");
#define PUSH_SETFIELD(what, name) push_##what(L), lua_setfield(L, -2, name)
	PUSH_SETFIELD(utf8class, "utf8class");
	PUSH_SETFIELD(category_map, "categories");
#ifdef USE_BIT_OPTIONS
	PUSH_SETFIELD(options, "options");
#endif
#undef PUSH_SETFIELD
	return 1;
}

#undef MAX_OPTION_BIT
#undef MASK_N