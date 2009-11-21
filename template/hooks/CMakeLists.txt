# the list of supported hooks
set(HOOKS
	"applypatch-msg"
	"commit-msg"
	"post-commit"
	"post-receive"
	"post-update"
	"pre-applypatch"
	"pre-commit"
	"pre-rebase"
	"prepare-commit-msg"
	"update")

# generate the hook scrips from the hook.in file and install
# them to the share/git-hooks/template/hooks/ directory
foreach(HOOK ${HOOKS})
	configure_file("hook.in"
		"${CMAKE_CURRENT_BINARY_DIR}/${HOOK}"
		@ONLY)
	install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/${HOOK}"
		DESTINATION "share/git-hooks/template/hooks")
endforeach(HOOK ${HOOKS})

