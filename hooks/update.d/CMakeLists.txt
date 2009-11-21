# install the hooks to share/git-hooks/update.d/
install(PROGRAMS "09verifyuser" "10verifyintegrity"
	DESTINATION "share/git-hooks/update.d")
