set( APPLICATION_NAME       "SwissDisk" )
set( APPLICATION_EXECUTABLE "SwissDisk" )
set( APPLICATION_DOMAIN     "swissdisk.com" )
set( APPLICATION_VENDOR     "maClara, LLC" )
set( APPLICATION_UPDATE_URL "https://www.swissdisk.com/client/desktop/" CACHE string "URL for updater" )

set( THEME_CLASS            "SwissDiskTheme" )
set( APPLICATION_REV_DOMAIN "com.swissdisk.desktopclient" )
set( WIN_SETUP_BITMAP_PATH  "${CMAKE_SOURCE_DIR}/SwissDisk/installer" )

set( MAC_INSTALLER_BACKGROUND_FILE "${CMAKE_SOURCE_DIR}/SwissDisk/installer/installer-background.png" CACHE STRING "The MacOSX installer background image")

set( THEME_INCLUDE          "${OEM_THEME_DIR}/theme.h" )
# set( APPLICATION_LICENSE    "${OEM_THEME_DIR}/license.txt )

option( WITH_CRASHREPORTER "Build crashreporter" OFF )
set( CRASHREPORTER_SUBMIT_URL "https://crash-reports.owncloud.org/submit" CACHE string "URL for crash repoter" )
set( CRASHREPORTER_ICON ":/swissdisk-icon.png" )

set( MIRALL_VERSION_SUFFIX "" )

# Update this with every release
set( MIRALL_VERSION_BUILD "1" )
