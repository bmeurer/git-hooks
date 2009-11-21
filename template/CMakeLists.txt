# install the sample configuration file config
# to the share/git-hooks/template/ folder
install(FILES "config"
	DESTINATION "share/git-hooks/template")

# install the default description file from the Git
# project to the share/git-hooks/template/ folder
install(FILES "description"
	DESTINATION "share/git-hooks/template")

add_subdirectory(hooks)
add_subdirectory(info)
