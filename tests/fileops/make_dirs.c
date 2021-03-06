#include <stic.h>

#include <unistd.h> /* chdir() rmdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() */

#include "../../src/compat/fs_limits.h"
#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"

#include "utils.h"

SETUP()
{
	set_to_sandbox_path(lwin.curr_dir, sizeof(lwin.curr_dir));
}

TEST(make_dirs_does_nothing_for_custom_view)
{
	int i;
	char path[] = "dir";
	char *paths[] = {path};

	if(is_path_absolute(TEST_DATA_PATH))
	{
		strcpy(lwin.curr_dir, TEST_DATA_PATH);
	}
	else
	{
		char cwd[PATH_MAX];
		assert_non_null(get_cwd(cwd, sizeof(cwd)));
		snprintf(lwin.curr_dir, sizeof(lwin.curr_dir), "%s/%s", cwd,
				TEST_DATA_PATH);
	}

	assert_int_equal(0, filter_init(&lwin.local_filter.filter, 0));

	flist_custom_start(&lwin, "test");
	flist_custom_add(&lwin, "existing-files/a");
	assert_true(flist_custom_finish(&lwin, CV_REGULAR, 0) == 0);

	fops_mkdirs(&lwin, -1, paths, 1, 0);
	assert_false(path_exists("dir", NODEREF));

	for(i = 0; i < lwin.list_rows; ++i)
	{
		fentry_free(&lwin, &lwin.dir_entry[i]);
	}
	dynarray_free(lwin.dir_entry);

	filter_dispose(&lwin.local_filter.filter);
}

TEST(make_dirs_does_nothing_for_duplicated_names)
{
	char path[] = "dir";
	char *paths[] = {path, path};

	fops_mkdirs(&lwin, -1, paths, 2, 0);
	assert_false(path_exists("dir", NODEREF));
}

TEST(make_dirs_does_nothing_for_empty_names)
{
	char path[] = "dir";
	char empty[] = "";
	char *paths[] = {path, empty};

	fops_mkdirs(&lwin, -1, paths, 2, 0);
	assert_false(path_exists("dir", NODEREF));
}

TEST(make_dirs_does_nothing_for_existing_names)
{
	char path[] = "not-exist";
	char empty[] = ".";
	char *paths[] = {path, empty};

	fops_mkdirs(&lwin, -1, paths, 2, 0);
	assert_false(path_exists("not-exist", NODEREF));
}

TEST(make_dirs_creates_one_dir)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[] = "dir";
		char *paths[] = {path};

		fops_mkdirs(&lwin, -1, paths, 1, 0);
		assert_true(is_dir(SANDBOX_PATH "/dir"));

		assert_success(rmdir(SANDBOX_PATH "/dir"));
	}
}

TEST(make_dirs_creates_sub_dirs_by_rel_path)
{
	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[] = "parent/child";
		char *paths[] = {path};

		fops_mkdirs(&lwin, -1, paths, 1, 1);
		assert_true(is_dir(SANDBOX_PATH "/parent/child"));

		assert_success(rmdir(SANDBOX_PATH "/parent/child"));
		assert_success(rmdir(SANDBOX_PATH "/parent"));
	}
}

TEST(make_dirs_creates_sub_dirs_by_abs_path)
{
	char cwd[PATH_MAX];
	assert_non_null(get_cwd(cwd, sizeof(cwd)));

	for(cfg.use_system_calls = 0; cfg.use_system_calls < 2;
			++cfg.use_system_calls)
	{
		char path[PATH_MAX];
		char *paths[] = {path};

		if(is_path_absolute(SANDBOX_PATH))
		{
			snprintf(path, sizeof(path), "%s/parent/child", SANDBOX_PATH);
		}
		else
		{
			snprintf(path, sizeof(path), "%s/%s/parent/child", cwd, SANDBOX_PATH);
		}

		fops_mkdirs(&lwin, -1, paths, 1, 1);
		assert_true(is_dir(SANDBOX_PATH "/parent/child"));

		assert_success(rmdir(SANDBOX_PATH "/parent/child"));
		assert_success(rmdir(SANDBOX_PATH "/parent"));
	}
}

TEST(make_dirs_considers_tree_structure)
{
	char path[] = "new-dir";
	char *paths[] = { path };

	view_setup(&lwin);

	create_empty_dir(SANDBOX_PATH "/dir");

	flist_load_tree(&lwin, SANDBOX_PATH);

	/* Set at to -1. */
	lwin.list_pos = 0;
	(void)fops_mkdirs(&lwin, -1, paths, 1, 0);

	/* Set at to desired position. */
	(void)fops_mkdirs(&lwin, 1, paths, 1, 0);

	/* Remove both files afterward to make sure they can both be created at the
	 * same time. */
	assert_success(rmdir(SANDBOX_PATH "/new-dir"));
	assert_success(rmdir(SANDBOX_PATH "/dir/new-dir"));

	assert_success(rmdir(SANDBOX_PATH "/dir"));

	view_teardown(&lwin);
}

TEST(check_by_absolute_path_is_performed_beforehand)
{
	char name_a[] = "a";
	char name_b[PATH_MAX + 8];
	char *names[] = { name_a, name_b };

	snprintf(name_b, sizeof(name_b), "%s/b", lwin.curr_dir);
	create_empty_dir(name_b);

	(void)fops_mkdirs(&lwin, -1, names, 2, 0);

	assert_failure(rmdir(SANDBOX_PATH "/a"));
	assert_success(rmdir(SANDBOX_PATH "/b"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
