#!/bin/sh
java -Xms512m -Xmx1024m \
    -cp "lib/build/libs/kotlin-checks-0.9.jar:ext/checkstyle-8.41.1-all.jar" \
    com.puppycrawl.tools.checkstyle.Main \
    -c config.xml \
    $@

