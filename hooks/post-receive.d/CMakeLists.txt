# install the hooks to share/git-hooks/post-receive.d/
install(PROGRAMS "99gitnotify"
	DESTINATION "share/git-hooks/post-receive.d")

