QT += widgets

TRANSLATIONS = translations/com2omp_ru.ts \
               translations/com2omp_be.ts

SOURCES += \
    main.cpp \
    Com2OmpWizard.cpp \
    chooseclassespage.cpp \
    chooseidlfilespage.cpp \
    wizardfields.cpp \
    mainprocesspage.cpp \
    maincalculationthread.cpp

HEADERS += \
    Com2OmpWizard.h \
    chooseclassespage.h \
    chooseidlfilespage.h \
    wizardfields.h \
    coclassdata.h \
    mainprocesspage.h \
    maincalculationthread.h

CONFIG += c++11

