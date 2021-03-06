#include <stic.h>

#include <sys/stat.h> /* chmod() */

#include <stdint.h> /* uint64_t */
#include <stdio.h> /* remove() */
#include <string.h> /* strcmp() */

#include "../../src/io/iop.h"
#include "../../src/io/ior.h"
#include "../../src/utils/env.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/utils.h"

#include "utils.h"

static int not_windows(void);
static int windows(void);

TEST(file_is_moved)
{
	create_empty_file(SANDBOX_PATH "/binary-data");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/binary-data",
			.arg2.dst = SANDBOX_PATH "/moved-binary-data",
		};
		assert_success(ior_mv(&args));
	}

	assert_false(file_exists(SANDBOX_PATH "/binary-data"));
	assert_true(file_exists(SANDBOX_PATH "/moved-binary-data"));

	remove(SANDBOX_PATH "/moved-binary-data");
}

TEST(empty_directory_is_moved)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-empty-dir",
		};
		assert_success(ior_mv(&args));
	}

	assert_false(is_dir(SANDBOX_PATH "/empty-dir"));
	assert_true(is_dir(SANDBOX_PATH "/moved-empty-dir"));

	delete_dir(SANDBOX_PATH "/moved-empty-dir");
}

TEST(non_empty_directory_is_moved)
{
	create_non_empty_dir(SANDBOX_PATH "/non-empty-dir", "a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-non-empty-dir",
		};
		assert_success(ior_mv(&args));
	}

	assert_true(file_exists(SANDBOX_PATH "/moved-non-empty-dir/a-file"));

	delete_tree(SANDBOX_PATH "/moved-non-empty-dir");
}

TEST(empty_nested_directory_is_moved)
{
	create_empty_nested_dir(SANDBOX_PATH "/non-empty-dir", "empty-nested-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-non-empty-dir",
		};
		assert_success(ior_mv(&args));
	}

	assert_true(is_dir(SANDBOX_PATH "/moved-non-empty-dir/empty-nested-dir"));

	delete_tree(SANDBOX_PATH "/moved-non-empty-dir");
}

TEST(non_empty_nested_directory_is_moved)
{
	create_non_empty_nested_dir(SANDBOX_PATH "/non-empty-dir", "nested-dir",
			"a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/non-empty-dir",
			.arg2.dst = SANDBOX_PATH "/moved-non-empty-dir",
		};
		assert_success(ior_mv(&args));
	}

	assert_false(file_exists(SANDBOX_PATH "/non-empty-dir/nested-dir/a-file"));
	assert_true(file_exists(SANDBOX_PATH
				"/moved-non-empty-dir/nested-dir/a-file"));

	delete_tree(SANDBOX_PATH "/moved-non-empty-dir");
}

TEST(fails_to_overwrite_file_by_default)
{
	create_empty_file(SANDBOX_PATH "/a-file");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read/two-lines",
			.arg2.dst = SANDBOX_PATH "/a-file",
		};
		assert_failure(ior_mv(&args));
	}

	delete_file(SANDBOX_PATH "/a-file");
}

TEST(fails_to_overwrite_dir_by_default)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/empty-dir",
		};
		assert_failure(ior_mv(&args));
	}

	delete_dir(SANDBOX_PATH "/empty-dir");
}

TEST(overwrites_file_when_asked)
{
	create_empty_file(SANDBOX_PATH "/a-file");
	clone_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/two-lines");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/two-lines",
			.arg2.dst = SANDBOX_PATH "/a-file",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_success(ior_mv(&args));
	}

	delete_file(SANDBOX_PATH "/a-file");
}

TEST(overwrites_dir_when_asked)
{
	create_empty_dir(SANDBOX_PATH "/dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/read",
		};
		assert_success(ior_cp(&args));
	}

	assert_success(chmod(SANDBOX_PATH "/read", 0700));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/dir",
			.arg3.crs = IO_CRS_REPLACE_ALL,
		};
		assert_success(ior_mv(&args));
	}

	{
		io_args_t args = {
			.arg1.path = SANDBOX_PATH "/dir",
		};
		assert_failure(iop_rmdir(&args));
	}

	delete_tree(SANDBOX_PATH "/dir");
}

TEST(appending_fails_for_directories)
{
	create_empty_dir(SANDBOX_PATH "/dir");

	{
		io_args_t args = {
			.arg1.src = TEST_DATA_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/read",
		};
		assert_success(ior_cp(&args));
	}

	assert_success(chmod(SANDBOX_PATH "/read", 0700));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/read",
			.arg2.dst = SANDBOX_PATH "/dir",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_failure(ior_mv(&args));
	}

	delete_dir(SANDBOX_PATH "/dir");

	delete_tree(SANDBOX_PATH "/read");
}

TEST(appending_works_for_files)
{
	uint64_t size;

	clone_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/two-lines");

	size = get_file_size(SANDBOX_PATH "/two-lines");

	clone_file(TEST_DATA_PATH "/read/two-lines", SANDBOX_PATH "/two-lines2");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/two-lines2",
			.arg2.dst = SANDBOX_PATH "/two-lines",
			.arg3.crs = IO_CRS_APPEND_TO_FILES,
		};
		assert_success(ior_mv(&args));
	}

	assert_int_equal(size, get_file_size(SANDBOX_PATH "/two-lines"));

	delete_file(SANDBOX_PATH "/two-lines");
}

TEST(directories_can_be_merged)
{
	create_empty_dir(SANDBOX_PATH "/first");
	create_empty_file(SANDBOX_PATH "/first/first-file");

	create_empty_dir(SANDBOX_PATH "/second");
	create_empty_file(SANDBOX_PATH "/second/second-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/first",
			.arg2.dst = SANDBOX_PATH "/second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_success(ior_mv(&args));
	}

	/* Original directory must be deleted. */
	assert_false(file_exists(SANDBOX_PATH "/first"));

	assert_true(file_exists(SANDBOX_PATH "/second/second-file"));
	assert_true(file_exists(SANDBOX_PATH "/second/first-file"));

	delete_tree(SANDBOX_PATH "/second");
}

TEST(nested_directories_can_be_merged)
{
	create_empty_dir(SANDBOX_PATH "/first");
	create_empty_dir(SANDBOX_PATH "/first/nested");
	create_empty_file(SANDBOX_PATH "/first/nested/first-file");

	create_empty_dir(SANDBOX_PATH "/second");
	create_empty_dir(SANDBOX_PATH "/second/nested");
	create_empty_file(SANDBOX_PATH "/second/nested/second-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/first",
			.arg2.dst = SANDBOX_PATH "/second",
			.arg3.crs = IO_CRS_REPLACE_FILES,
		};
		assert_success(ior_mv(&args));
	}

	/* Original directory must be deleted. */
	assert_false(file_exists(SANDBOX_PATH "/first/nested"));
	assert_false(file_exists(SANDBOX_PATH "/first"));

	assert_true(file_exists(SANDBOX_PATH "/second/nested/second-file"));
	assert_true(file_exists(SANDBOX_PATH "/second/nested/first-file"));

	delete_tree(SANDBOX_PATH "/second");
}

TEST(fails_to_move_directory_inside_itself)
{
	create_empty_dir(SANDBOX_PATH "/empty-dir");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/empty-dir",
			.arg2.dst = SANDBOX_PATH "/empty-dir/empty-dir-copy",
		};
		assert_failure(ior_mv(&args));
	}

	delete_dir(SANDBOX_PATH "/empty-dir");
}

/* Creating symbolic links on Windows requires administrator rights. */
TEST(symlink_is_symlink_after_move, IF(not_windows))
{
	{
		io_args_t args = {
			.arg1.path = TEST_DATA_PATH "/read/two-lines",
			.arg2.target = SANDBOX_PATH "/sym-link",
		};
		assert_success(iop_ln(&args));
	}

	assert_true(is_symlink(SANDBOX_PATH "/sym-link"));

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/sym-link",
			.arg2.dst = SANDBOX_PATH "/moved-sym-link",
		};
		assert_success(ior_mv(&args));
	}

	assert_false(is_symlink(SANDBOX_PATH "/sym-link"));
	assert_true(is_symlink(SANDBOX_PATH "/moved-sym-link"));

	delete_file(SANDBOX_PATH "/moved-sym-link");
}

/* Case insensitive renames are easier to check on Windows. */
TEST(case_insensitive_rename, IF(windows))
{
	create_empty_file(SANDBOX_PATH "/a-file");

	{
		io_args_t args = {
			.arg1.src = SANDBOX_PATH "/a-file",
			.arg2.dst = SANDBOX_PATH "/A-file",
		};
		assert_success(ior_mv(&args));
	}

	delete_file(SANDBOX_PATH "/A-file");
}

static int
not_windows(void)
{
	return get_env_type() != ET_WIN;
}

static int
windows(void)
{
	return (env_get("_") == NULL || strcmp(env_get("_"), "/usr/bin/wine") != 0)
	    && get_env_type() == ET_WIN;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
