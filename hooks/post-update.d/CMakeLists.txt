# install the hooks to share/git-hooks/post-update.d/
install(PROGRAMS "99updateserverinfo"
	DESTINATION "share/git-hooks/post-update.d")
