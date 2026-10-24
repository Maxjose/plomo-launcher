@echo off
rem Builds the PLOMO launcher (Release). Output: LAUNCHER\source\build-plomo-ninja2\Release\PLOMO.exe
rem Needs Visual Studio Build Tools (C++) and JDK 17; Qt and vcpkg live under LAUNCHER\.tools.
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "%~dp0..\source"
rem JDK 17: with JDK 21 the bundled Java 7 sources do not compile.
set "J=C:/Program Files/Eclipse Adoptium/jdk-17.0.20.101-hotspot/bin"
rem The URLs below are forced because CMake keeps old values in its cache.
cmake -S . -B build-plomo-ninja2 -DLauncher_LOGIN_CALLBACK_URL=https://playplomo.com/login-success/ -DLauncher_META_URL=https://cdn.playplomo.com/meta/v1/ -ULauncher_NEWS_RSS_URL -ULauncher_NEWS_OPEN_URL -ULauncher_WIKI_URL -ULauncher_HELP_URL -ULauncher_LEGACY_FMLLIBS_BASE_URL -ULauncher_UPDATER_GITHUB_REPO -ULauncher_BUG_TRACKER_URL -ULauncher_TRANSLATIONS_URL -ULauncher_TRANSLATION_FILES_URL -ULauncher_DISCORD_URL -DJava_JAVAC_EXECUTABLE="%J%/javac.exe" -DJava_JAVA_EXECUTABLE="%J%/java.exe" -DJava_JAR_EXECUTABLE="%J%/jar.exe" -DJava_JAVADOC_EXECUTABLE="%J%/javadoc.exe" -DJava_IDLJ_EXECUTABLE=IDLJ-NOTFOUND || exit /b 1
cmake --build build-plomo-ninja2 --config Release
