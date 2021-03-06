#include <stic.h>

#include <unistd.h> /* chdir() rmdir() symlink() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* fclose() fopen() free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcsdup() */

#include "../../src/compat/fs_limits.h"
#include "../../src/compat/os.h"
#include "../../src/cfg/config.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
#include "../../src/engine/functions.h"
#include "../../src/engine/options.h"
#include "../../src/engine/variables.h"
#include "../../src/int/path_env.h"
#include "../../src/modes/cmdline.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/bmarks.h"
#include "../../src/builtin_functions.h"
#include "../../src/cmd_core.h"

#include "utils.h"

#define ASSERT_COMPLETION(initial, expected) \
	do \
	{ \
		prepare_for_line_completion(initial); \
		assert_success(line_completion(&stats)); \
		assert_wstring_equal(expected, stats.line); \
	} \
	while (0)

#define ASSERT_NEXT_MATCH(str) \
	do \
	{ \
		char *const buf = vle_compl_next(); \
		assert_string_equal((str), buf); \
		free(buf); \
	} \
	while (0)

static void dummy_handler(OPT_OP op, optval_t val);
static int dquotes_allowed_in_paths(void);

static line_stats_t stats;
static char *saved_cwd;

SETUP()
{
	static int option_changed;
	optval_t def = { .str_val = "/tmp" };

	cfg.slow_fs_list = strdup("");

	init_builtin_functions();

	stats.line = wcsdup(L"set ");
	stats.index = wcslen(stats.line);
	stats.curs_pos = 0;
	stats.len = stats.index;
	stats.cmd_pos = -1;
	stats.complete_continue = 0;
	stats.history_search = 0;
	stats.line_buf = NULL;
	stats.complete = &complete_cmd;

	curr_view = &lwin;

	init_commands();

	execute_cmd("command bar a");
	execute_cmd("command baz b");
	execute_cmd("command foo c");

	init_options(&option_changed, NULL);
	add_option("fusehome", "fh", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			&dummy_handler, def);
	add_option("path", "pt", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			&dummy_handler, def);
	add_option("path", "pt", "descr", OPT_STR, OPT_LOCAL, 0, NULL, &dummy_handler,
			def);

	saved_cwd = save_cwd();
	assert_success(chdir(TEST_DATA_PATH "/compare"));
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "compare", saved_cwd);
}

TEARDOWN()
{
	restore_cwd(saved_cwd);

	update_string(&cfg.slow_fs_list, NULL);

	free(stats.line);
	reset_cmds();
	clear_options();

	function_reset_all();
}

static void
dummy_handler(OPT_OP op, optval_t val)
{
}

TEST(vim_like_completion)
{
	vle_compl_reset();
	assert_int_equal(0, complete_cmd("e", NULL));
	ASSERT_NEXT_MATCH("echo");
	ASSERT_NEXT_MATCH("edit");
	ASSERT_NEXT_MATCH("else");
	ASSERT_NEXT_MATCH("elseif");
	ASSERT_NEXT_MATCH("empty");
	ASSERT_NEXT_MATCH("endif");
	ASSERT_NEXT_MATCH("execute");
	ASSERT_NEXT_MATCH("exit");
	ASSERT_NEXT_MATCH("e");

	vle_compl_reset();
	assert_int_equal(0, complete_cmd("vm", NULL));
	ASSERT_NEXT_MATCH("vmap");
	ASSERT_NEXT_MATCH("vmap");

	vle_compl_reset();
	assert_int_equal(0, complete_cmd("j", NULL));
	ASSERT_NEXT_MATCH("jobs");
	ASSERT_NEXT_MATCH("jobs");
}

TEST(leave_spaces_at_begin)
{
	vle_compl_reset();
	assert_int_equal(1, complete_cmd(" qui", NULL));
	ASSERT_NEXT_MATCH("quit");
	ASSERT_NEXT_MATCH("quit");
}

TEST(only_user)
{
	vle_compl_reset();
	assert_int_equal(8, complete_cmd("command ", NULL));
	ASSERT_NEXT_MATCH("bar");

	vle_compl_reset();
	assert_int_equal(9, complete_cmd(" command ", NULL));
	ASSERT_NEXT_MATCH("bar");

	vle_compl_reset();
	assert_int_equal(10, complete_cmd("  command ", NULL));
	ASSERT_NEXT_MATCH("bar");
}

static void
prepare_for_line_completion(const wchar_t str[])
{
	free(stats.line);
	stats.line = wcsdup(str);
	stats.len = wcslen(stats.line);
	stats.index = stats.len;
	stats.complete_continue = 0;

	vle_compl_reset();
}

TEST(test_set_completion)
{
	ASSERT_COMPLETION(L"set ", L"set all");
}

TEST(no_sdquoted_completion_does_nothing)
{
	ASSERT_COMPLETION(L"command '", L"command '");
}

TEST(spaces_escaping_leading)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "spaces-in-names", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"touch \\ ", L"touch \\ begins-with-space");
}

TEST(spaces_escaping_everywhere)
{
	assert_success(chdir("../spaces-in-names"));

	/* Whether trailing space is there depends on file system and OS. */
	if(access("\\ spaces\\ everywhere\\ ", F_OK) == 0)
	{
		ASSERT_COMPLETION(L"touch \\ s", L"touch \\ spaces\\ everywhere\\ ");
	}
	/* Only one condition is true, but don't use else to make one of asserts fail
	 * if there are two files somehow. */
	if(access("\\ spaces\\ everywhere", F_OK) == 0)
	{
		ASSERT_COMPLETION(L"touch \\ s", L"touch \\ spaces\\ everywhere");
	}
}

TEST(spaces_escaping_trailing)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "spaces-in-names", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	/* Whether trailing space is there depends on file system and OS. */
	if(access("ends-with-space\\ ", F_OK) == 0)
	{
		ASSERT_COMPLETION(L"touch e", L"touch ends-with-space\\ ");
	}
	/* Only one condition is true, but don't use else to make one of asserts fail
	 * if there are too files somehow. */
	if(access("ends-with-space", F_OK) == 0)
	{
		ASSERT_COMPLETION(L"touch e", L"touch ends-with-space");
	}
}

TEST(spaces_escaping_middle)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "spaces-in-names", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"touch s", L"touch spaces\\ in\\ the\\ middle");
}

TEST(squoted_completion)
{
	ASSERT_COMPLETION(L"touch '", L"touch 'a");
}

TEST(squoted_completion_escaping)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "quotes-in-names", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"touch 's-quote", L"touch 's-quote-''-in-name");
}

TEST(dquoted_completion)
{
	ASSERT_COMPLETION(L"touch 'b", L"touch 'b");
}

TEST(dquoted_completion_escaping, IF(dquotes_allowed_in_paths))
{
	assert_success(chdir(SANDBOX_PATH));
	strcpy(curr_view->curr_dir, SANDBOX_PATH);

	create_file("d-quote-\"-in-name");
	create_file("d-quote-\"-in-name-2");
	create_file("d-quote-\"-in-name-3");

	ASSERT_COMPLETION(L"touch \"d-quote", L"touch \"d-quote-\\\"-in-name");

	assert_success(unlink("d-quote-\"-in-name"));
	assert_success(unlink("d-quote-\"-in-name-2"));
	assert_success(unlink("d-quote-\"-in-name-3"));
}

TEST(last_match_is_properly_escaped)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "quotes-in-names", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"touch 's-quote-''-in", L"touch 's-quote-''-in-name");
	ASSERT_NEXT_MATCH("s-quote-''-in-name-2");
	ASSERT_NEXT_MATCH("s-quote-''-in");
}

TEST(emark_cmd_escaping)
{
	ASSERT_COMPLETION(L"", L"!");
	ASSERT_NEXT_MATCH("alink");
}

TEST(winrun_cmd_escaping)
{
	ASSERT_COMPLETION(L"winrun ", L"winrun $");
	ASSERT_NEXT_MATCH("%");
	ASSERT_NEXT_MATCH(",");
	ASSERT_NEXT_MATCH(".");
	ASSERT_NEXT_MATCH("^");
}

TEST(help_cmd_escaping)
{
	cfg.use_vim_help = 1;
	ASSERT_COMPLETION(L"help vifm-", L"help vifm-!!");
}

TEST(root_is_completed)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	prepare_for_line_completion(L"cd /");
	assert_success(line_completion(&stats));
	assert_true(wcscmp(L"cd /", stats.line) != 0);
}

TEST(dirs_are_completed_with_trailing_slash)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"cd r", L"cd read/");
	ASSERT_NEXT_MATCH("rename/");
	ASSERT_NEXT_MATCH("r");
	ASSERT_NEXT_MATCH("read/");
}

TEST(function_name_completion)
{
	ASSERT_COMPLETION(L"echo e", L"echo executable(");
	ASSERT_NEXT_MATCH("expand(");
	ASSERT_NEXT_MATCH("e");
}

TEST(percent_completion)
{
	/* One percent symbol. */

	ASSERT_COMPLETION(L"cd %", L"cd %%");
	ASSERT_NEXT_MATCH("%%");
	ASSERT_NEXT_MATCH("%%");

	/* Two percent symbols. */

	ASSERT_COMPLETION(L"cd %%", L"cd %%");
	ASSERT_NEXT_MATCH("%%");
	ASSERT_NEXT_MATCH("%%");

	/* Three percent symbols. */

	ASSERT_COMPLETION(L"cd %%%", L"cd %%%%");
	ASSERT_NEXT_MATCH("%%%%");
	ASSERT_NEXT_MATCH("%%%%");
}

TEST(abbreviations)
{
	vle_abbr_reset();
	assert_success(vle_abbr_add(L"lhs", L"rhs"));

	ASSERT_COMPLETION(L"cabbrev l", L"cabbrev lhs");
	ASSERT_COMPLETION(L"cnoreabbrev l", L"cnoreabbrev lhs");
	ASSERT_COMPLETION(L"cunabbrev l", L"cunabbrev lhs");
	ASSERT_COMPLETION(L"cabbrev l l", L"cabbrev l l");

	vle_abbr_reset();
}

TEST(bang_exec_completion)
{
	char *const original_path_env = strdup(env_get("PATH"));

	restore_cwd(saved_cwd);
	assert_success(chdir(SANDBOX_PATH));
	saved_cwd = save_cwd();

	env_set("PATH", saved_cwd);
	update_path_env(1);

	create_executable("exec-for-completion" EXE_SUFFIX);

	ASSERT_COMPLETION(L"!exec-for-com", L"!exec-for-completion" EXE_SUFFIXW);

	assert_success(unlink("exec-for-completion" EXE_SUFFIX));

	env_set("PATH", original_path_env);
	update_path_env(1);
	free(original_path_env);
}

TEST(bang_abs_path_completion)
{
	wchar_t input[PATH_MAX];
	wchar_t cmd[PATH_MAX];
	char cwd[PATH_MAX];
	wchar_t *wcwd;

	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
	assert_success(chdir(SANDBOX_PATH));

	assert_true(get_cwd(cwd, sizeof(cwd)) == cwd);
	wcwd = to_wide(cwd);

	create_executable("exec-for-completion" EXE_SUFFIX);

	vifm_swprintf(input, ARRAY_LEN(input),
			L"!%" WPRINTF_WSTR L"/exec-for-compl", wcwd);
	vifm_swprintf(cmd, ARRAY_LEN(cmd),
			L"!%" WPRINTF_WSTR L"/exec-for-completion" EXE_SUFFIXW, wcwd);

	ASSERT_COMPLETION(input, cmd);

	assert_int_equal(2, vle_compl_get_count());

	assert_success(unlink("exec-for-completion" EXE_SUFFIX));

	free(wcwd);
}

TEST(tilde_is_completed_after_emark)
{
	make_abs_path(cfg.home_dir, sizeof(cfg.home_dir), TEST_DATA_PATH, "",
			saved_cwd);
	ASSERT_COMPLETION(L"!~/", L"!~/compare/");
}

TEST(bmark_tags_are_completed)
{
	bmarks_clear();

	assert_success(exec_commands("bmark! fake/path1 tag1", &lwin, CIT_COMMAND));

	ASSERT_COMPLETION(L"bmark tag", L"bmark tag1");
	ASSERT_COMPLETION(L"bmark! fake/path2 tag", L"bmark! fake/path2 tag1");
	ASSERT_COMPLETION(L"bmark! fake/path2 ../", L"bmark! fake/path2 ../");
	ASSERT_COMPLETION(L"bmark! fake/path2 ", L"bmark! fake/path2 tag1");
}

TEST(bmark_path_is_completed)
{
	bmarks_clear();

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir), SANDBOX_PATH,
			"", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));
	create_executable("exec-for-completion" EXE_SUFFIX);

	/* ASSERT_COMPLETION(L"bmark! exec", L"bmark! exec-for-completion" EXE_SUFFIX); */

	assert_success(unlink("exec-for-completion" EXE_SUFFIX));
}

TEST(delbmark_tags_are_completed)
{
	bmarks_clear();

	assert_success(exec_commands("bmark! fake/path1 tag1", &lwin, CIT_COMMAND));

	ASSERT_COMPLETION(L"delbmark ../", L"delbmark ../");
}

TEST(selective_sync_completion)
{
	ASSERT_COMPLETION(L"sync! a", L"sync! all");
	ASSERT_COMPLETION(L"sync! ../", L"sync! ../");
}

TEST(colorscheme_completion)
{
	make_abs_path(cfg.colors_dir, sizeof(cfg.colors_dir), TEST_DATA_PATH,
			"scripts", saved_cwd);
	ASSERT_COMPLETION(L"colorscheme set-", L"colorscheme set-env");
	ASSERT_COMPLETION(L"colorscheme set-env ../",
			L"colorscheme set-env ../compare/");
	ASSERT_COMPLETION(L"colorscheme ../", L"colorscheme ../");

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	ASSERT_COMPLETION(L"colorscheme set-env ", L"colorscheme set-env compare/");
}

TEST(wincmd_completion)
{
	ASSERT_COMPLETION(L"wincmd ", L"wincmd +");
	ASSERT_COMPLETION(L"wincmd + ", L"wincmd + ");
}

TEST(grep_completion)
{
	ASSERT_COMPLETION(L"grep -", L"grep -");
	ASSERT_COMPLETION(L"grep .", L"grep .");
	ASSERT_COMPLETION(L"grep -o ..", L"grep -o ../");

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"grep -o ", L"grep -o compare/");
}

TEST(find_completion)
{
#ifdef _WIN32
	/* Windows escaping code doesn't prepend "./". */
	ASSERT_COMPLETION(L"find -", L"find -");
#else
	ASSERT_COMPLETION(L"find -", L"find ./-");
#endif

	ASSERT_COMPLETION(L"find ..", L"find ../");
	ASSERT_COMPLETION(L"find . .", L"find . .");

	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	ASSERT_COMPLETION(L"find ", L"find compare/");
}

TEST(aucmd_events_are_completed)
{
	ASSERT_COMPLETION(L"autocmd ", L"autocmd DirEnter");
	ASSERT_COMPLETION(L"autocmd Dir", L"autocmd DirEnter");
	ASSERT_COMPLETION(L"autocmd! Dir", L"autocmd! DirEnter");
	ASSERT_COMPLETION(L"autocmd DirEnter ", L"autocmd DirEnter ");
}

TEST(prefixless_option_name_is_completed)
{
	ASSERT_COMPLETION(L"echo &", L"echo &fusehome");
	assert_success(line_completion(&stats));
	assert_wstring_equal(L"echo &path", stats.line);
}

TEST(prefixed_global_option_name_is_completed)
{
	ASSERT_COMPLETION(L"echo &g:f", L"echo &g:fusehome");
}

TEST(prefixed_local_option_name_is_completed)
{
	ASSERT_COMPLETION(L"echo &l:p", L"echo &l:path");
}

TEST(autocmd_name_completion_is_case_insensitive)
{
	ASSERT_COMPLETION(L"autocmd dir", L"autocmd DirEnter");
}

TEST(highlight_is_completed)
{
	ASSERT_COMPLETION(L"hi ", L"hi Border");
	ASSERT_COMPLETION(L"hi wi", L"hi WildMenu");
	ASSERT_COMPLETION(L"hi WildMenu cter", L"hi WildMenu cterm");
}

TEST(case_override_of_paths)
{
	make_abs_path(curr_view->curr_dir, sizeof(curr_view->curr_dir),
			TEST_DATA_PATH, "existing-files", saved_cwd);
	assert_success(chdir(curr_view->curr_dir));

	cfg.ignore_case = 0;
	cfg.case_override = CO_PATH_COMPL;
	cfg.case_ignore = CO_PATH_COMPL;

	ASSERT_COMPLETION(L"edit A", L"edit a");

	cfg.case_override = 0;
	cfg.case_ignore = 0;
}

TEST(envvars_are_completed_for_edit)
{
	env_set("RRRRRARE_VARIABLE1", "1");
	env_set("RRRRRARE_VARIABLE2", "2");

	ASSERT_COMPLETION(L"edit $RRRRRARE_VARIA", L"edit $RRRRRARE_VARIABLE1");
}

TEST(builtinvars_are_completed_for_echo)
{
	init_variables();
	assert_success(setvar("v:test", var_from_bool(1)));
	ASSERT_COMPLETION(L"echo v:", L"echo v:test");
	clear_variables();
}

TEST(select_is_completed)
{
	env_set("RRRRRARE_VARIABLE1", "1");
	env_set("RRRRRARE_VARIABLE2", "2");

	ASSERT_COMPLETION(L"select $RRRRRARE_VARIA", L"select $RRRRRARE_VARIA");
	ASSERT_COMPLETION(L"select !/$RRRRRARE_VARIA", L"select !/$RRRRRARE_VARIA");
	ASSERT_COMPLETION(L"select !cmd some-arg", L"select !cmd some-arg");

	/* Check that not memory violations occur here. */
	prepare_for_line_completion(L"select !cmd ");
	assert_success(line_completion(&stats));

	ASSERT_COMPLETION(L"select!!$RRRRRARE_VARIA", L"select!!$RRRRRARE_VARIABLE1");
	ASSERT_COMPLETION(L"unselect !cat $RRRRRARE_VARIA",
			L"unselect !cat $RRRRRARE_VARIABLE1");
}

TEST(compare_is_completed)
{
	ASSERT_COMPLETION(L"compare by", L"compare bycontents");
	ASSERT_COMPLETION(L"compare bysize list", L"compare bysize listall");
}

TEST(symlinks_in_paths_are_not_resolved, IF(not_windows))
{
	/* symlink() is not available on Windows, but the rest of the code is fine. */
#ifndef _WIN32
	assert_success(symlink(TEST_DATA_PATH "/compare", SANDBOX_PATH "/dir-link"));
#endif

	assert_success(chdir(SANDBOX_PATH "/dir-link"));
	strcpy(curr_view->curr_dir, SANDBOX_PATH "/dir-link");

	ASSERT_COMPLETION(L"cd ../d", L"cd ../dir-link/");

	assert_success(remove(SANDBOX_PATH "/dir-link"));
}

static int
dquotes_allowed_in_paths(void)
{
	if(os_mkdir(SANDBOX_PATH "/a\"b", 0700) == 0)
	{
		assert_success(rmdir(SANDBOX_PATH "/a\"b"));
		return 1;
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
