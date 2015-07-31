#ifndef COCLASSDATA
#define COCLASSDATA

#include <QString>
#include <QVector>
#include <QSet>


struct InterfaceData
{
    InterfaceData() : is_com_interface(true){}        // com by default (if not predefined)

    QString          name;
    QString          definition;
    QVector<QString> types_in_definition;
    QString          header_file_name;
    bool             is_default;
    QString          absolute_file_name;
    bool             is_com_interface;
    QString          _names_of_methods;
};

struct ComClassData
{
    QString                idl_name;
    QString                idl_definition;
    QVector<InterfaceData> interfaces;
    QString                cpp_lang_name;
    QString                OID;
    QString                CLSID;
    QSet<QString>          definitions_of_types;
    bool                   is_dual_object;
    QString                SQL_convert_script;
};

#endif // COCLASSDATA

