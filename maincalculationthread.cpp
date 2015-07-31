#include "maincalculationthread.h"
#include "mainprocesspage.h"
#include "Com2OmpWizard.h"
#include <QFile>
#include <QRegularExpressionMatchIterator>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

static const QString ERROR = "ERROR : ";

MainCalculationThread::MainCalculationThread(QObject *parent)
    : QThread(parent)
    , m_answer_is_yes(true)
    , m_always_yes(false)
    , m_always_yes_asked(false)
{

}

void MainCalculationThread::setCalculationData(QVector<ComClassData> allClassesInIdl, QVector<QString> selectedClasses, QString clsIdlFilename, QString dataIdlFilename)
{
    m_selected_classes  = selectedClasses;
    m_CLSID_filename    = clsIdlFilename;
    m_data_IDL_filename = dataIdlFilename;

    for ( const ComClassData& it : allClassesInIdl )
        m_all_classes_in_IDL.insert(it.idl_name, it);
}

void MainCalculationThread::resume()
{
    m_sync.lock();
    m_pause = false;
    m_sync.unlock();
    m_pause_cond.wakeAll();
}

void MainCalculationThread::pause()
{
    m_sync.lock();
    m_pause = true;
    m_sync.unlock();
}

void MainCalculationThread::answerUserYesOrNoSlot(bool ignore)
{
    m_answer_is_yes = ignore;
    resume();
}

void MainCalculationThread::run()
{
    for( const QString& IDL_class_name : m_selected_classes )
    {
        ComClassData& current_class = m_all_classes_in_IDL[ IDL_class_name ];

        stringToLog(tr("Processing class ") + current_class.idl_name);

        current_class.OID = "OID_" + current_class.idl_name;

        QString cls_file_contents = readFile(m_CLSID_filename);
        QString data_file_contents = readFile(m_data_IDL_filename);;

        bool are_interfaces_ok = true;
        QMap<QString,bool> data_type_moving_agree;

        for( InterfaceData& interface : current_class.interfaces )
        {
            stringToLog(tr("Processing interface ") + interface.name);

            interface.definition = findInterfaceDescription(cls_file_contents,interface.name);

            if( interface.definition.isEmpty() && !searchInterfaceInPredefinedMakeSql(interface, current_class) )
            {
                doAskYesOrNo(tr("Interface ") + interface.name + tr(" not found. Ignore?"));

                if( !m_answer_is_yes )
                {
                    stringToLog( ERROR + tr("Have not founded the description if interface ")
                                 + interface.name + tr(" in file ") + m_CLSID_filename
                                 + tr(". Class convertion ") + current_class.idl_name + tr(" aborted.(;") );
                    are_interfaces_ok = false;
                    break;
                }
            }

            if( interface.definition.contains("IDispatch") )
            {
                stringToLog( ERROR + "Interface " + interface.name + tr(" inherits IDispatch. In file ")
                             + m_CLSID_filename + tr(". Class convertion ")
                             + current_class.idl_name + tr(" aborted.(;") );
                are_interfaces_ok = false;
                break;
            }

            QVector<QString> data_names;
            findIdlDatatypes(interface.definition, data_names);

            for(const auto& type : data_names)
            {
                if( data_type_moving_agree.contains(type) && data_type_moving_agree.value(type) == true )
                {
                    interface.types_in_definition.push_back(type);
                    continue;
                }

                if( !m_always_yes )
                {
                    doAskYesOrNo(tr("Do you want to move type definition of ") + type + tr(" for interface ")
                                 + interface.name + tr(" from idl to *Data.h file?"));

                    if( !m_always_yes_asked )
                    {
                        bool temp_yes = m_answer_is_yes;
                        doAskYesOrNo(tr("Do you want ALWAYS YES for moving type definitions?"));
                        m_always_yes = m_answer_is_yes;
                        m_always_yes_asked = true;
                        m_answer_is_yes = temp_yes;
                    }
                }
                else
                    m_answer_is_yes = true;

                if( m_answer_is_yes )
                {
                    interface.types_in_definition.push_back(type);
                }

                data_type_moving_agree.insert(type,m_answer_is_yes);
            }

            if( !findIdlDatatypesDescription(data_file_contents, interface.types_in_definition, current_class.definitions_of_types) )
            {
                stringToLog( ERROR + tr("Have not founded the description of all needed types for ")
                             + interface.name + tr(" in file ") + m_data_IDL_filename + tr(". Class convertion ")
                             + current_class.idl_name + tr(" aborted.(;") );
                interface.definition.clear();
                are_interfaces_ok = false;
                break;
            }
        }

        if( !are_interfaces_ok )
            continue;

        QFileInfo cls_file_info(m_CLSID_filename);
        QDir solution_dir = cls_file_info.dir();
        solution_dir.cdUp();

        // searching of class definition file (*.h)

        stringToLog( tr("Searching *.h for ") + current_class.idl_name );
        QList<QFileInfo> matched_hfiles;

        if( m_project_dir.dirName() == "." )
            m_project_dir = solution_dir;

        QDirIterator it(m_project_dir.absolutePath(), QStringList() << "*.h", QDir::Files, QDirIterator::Subdirectories);
        QRegularExpression find_COM_inheritance_regex("public(.*)CLSID_" + current_class.idl_name + "[^\\w]");

        while ( it.hasNext() )
        {
            QString header_file_name = it.next();
            QString cls_header_file_contents = readFile(header_file_name);
            if( cls_header_file_contents.contains(find_COM_inheritance_regex) )
                matched_hfiles << QFileInfo(header_file_name);
        }

        if( matched_hfiles.size() > 1 || matched_hfiles.size() == 0 )
        {
            stringToLog( ERROR + tr("More than 1 headers found or not found at all for") + current_class.idl_name + tr(" in ")
                         + m_project_dir.absolutePath() + tr(". Class convertion ")
                         + current_class.idl_name + tr(" aborted.(;") );
            continue;
        }

        m_project_dir = matched_hfiles.front().dir();
        // searching of class implementation file (*.cpp)

        stringToLog( tr("Searching *.cpp for ") + current_class.idl_name );
        QString cpp_file_path = m_project_dir.absolutePath() + "/" + matched_hfiles.front().baseName() + ".cpp";

        QFile try_cpp_file_read(cpp_file_path);
        if( !try_cpp_file_read.open(QIODevice::ReadOnly|QIODevice::Text) )
        {
            stringToLog(ERROR + tr("Cannot open file ") + cpp_file_path + tr(" in ") + m_project_dir.absolutePath()
                        + tr(". Class convertion ") + current_class.idl_name + tr(" aborted.(;"));
            continue;
        }
        try_cpp_file_read.close();

        // searching of the files was successful
        QFileInfo cpp_file_info(try_cpp_file_read);
        QFileInfo h_file_info( matched_hfiles.front() );
        QFileInfo data_file_info;

        if( !current_class.definitions_of_types.isEmpty() )
        {
            // creating of file with structures
            QString dataFileName = h_file_info.absolutePath() + "/" + current_class.idl_name + "Data.h";
            stringToLog("Creating " + dataFileName);

            data_file_info.setFile(dataFileName);

            QString stream_string;
            QTextStream data_file_stream(&stream_string);

            data_file_stream << "#pragma once\n\n"
                             << "#include <" << QFileInfo(m_data_IDL_filename).baseName() << ".h>\n\n";

            for( auto s : current_class.definitions_of_types )
                data_file_stream << s << "\n\n";

            writeFile(dataFileName, *data_file_stream.string());
        }

        //creating of files with descriptions of interfaces
        for( InterfaceData& interface : current_class.interfaces )
        {
            if( interface.definition.isEmpty() )
                continue;

            interface.is_com_interface = false;   // will be converted
            interface.header_file_name = interface.name.mid(1) + "I.h";
            stringToLog(tr("Creating ") + interface.header_file_name);
            QString stream_string;
            QTextStream interfaces_file_stream(&stream_string);

            interfaces_file_stream << "#pragma once\n\n"
               << "#include \"OIDs.h\"\n";

            if(!current_class.definitions_of_types.isEmpty())
                interfaces_file_stream << "#include \"" << data_file_info.fileName() << "\"\n";
            interfaces_file_stream << "\n";

            interfaces_file_stream << convertInterfaceDescToOmp(interface.definition);

            interfaces_file_stream << "\n\nDECLARE_DEFAULT_OID( " << interface.name + ", " + current_class.OID + " );";

            interface.absolute_file_name = h_file_info.absolutePath() + "/" + interface.header_file_name;
            writeFile(interface.absolute_file_name, *interfaces_file_stream.string());
        }

        stringToLog(tr("converting Class Header To Omp format"));

        QVector<QString> converted_methods;
        convertClassHeaderToOmp(h_file_info, current_class,converted_methods);

        stringToLog(tr("converting Class implementation To Omp format"));
        convertClassImplementationToOmp(cpp_file_info, current_class,converted_methods);

        // working with OID

        stringToLog(tr("Searching <project_name>.cpp file to replace OBJECT_ENTRY to OMP_OBJECT_ENTRY"));
        QDirIterator it_for_oid_in_project(h_file_info.absolutePath(), QStringList() << "*.cpp", QDir::Files, QDirIterator::Subdirectories);

        bool break_ = false;
        while ( it_for_oid_in_project.hasNext() && !break_ )
        {
            QString file_name = it_for_oid_in_project.next();
            QString file_contents = readFile(file_name);

            QRegularExpression expr("\\n.*OBJECT_ENTRY.*CLSID_" + current_class.idl_name + "[^\\w].*");

            if( file_contents.contains(expr) )
            {
                file_contents.replace(expr,"");
                file_contents.replace("END_OMP_OBJECT_MAP", "  OMP_OBJECT_ENTRY( " + current_class.OID + ", " + current_class.cpp_lang_name + " )\nEND_OMP_OBJECT_MAP");

                writeFile(file_name, file_contents);

                QRegularExpression dll_oid_expr("BEGIN_OMP_OBJECT_MAP\\(\\s*(?<oid_of_dll>\\w+)\\s*\\)");
                m_OID_of_DLL = dll_oid_expr.globalMatch(file_contents).next().captured("oid_of_dll");
                break_ = true;
            }
        }

        stringToLog(tr("Searching oids.h"));
        QDirIterator it_for_oids_h(solution_dir.absolutePath(), QStringList() << "oids.h", QDir::Files, QDirIterator::Subdirectories);

        if ( it_for_oids_h.hasNext() )
        {
            QString file_name = it_for_oids_h.next();
            QString oids_file_contents = readFile(file_name);

            QRegularExpression dll_num_range_regex(".*" + m_OID_of_DLL + "\\s*(?<dll_num>\\d+)\\s*\\*\\s*OBJECTS_PER_DLL");
            int dll_num_range = dll_num_range_regex.match(oids_file_contents).captured("dll_num").toInt() * 10000;

            QString OID_in_DLL_pattern = ".*" + m_OID_of_DLL + "\\s*\\+\\s*(?<num_offset>\\d+)\\s*\\)";
            QRegularExpression OID_in_DLL_regex(OID_in_DLL_pattern);
            QRegularExpressionMatchIterator OID_in_DLL_iter = OID_in_DLL_regex.globalMatch(oids_file_contents);

            QString captured;
            QString num;
            while( OID_in_DLL_iter.hasNext() )
            {
                QRegularExpressionMatch match = OID_in_DLL_iter.next();
                captured = match.captured();
                num = match.captured("num_offset");
            }
            int num_ = num.toInt();
            num = QString::number(++num_);
            oids_file_contents.replace(captured, captured + "\n#define " + current_class.OID + "  (" + m_OID_of_DLL + " + " + num + ")" );
            num = QString::number(num_ + dll_num_range);
            current_class.SQL_convert_script.replace(current_class.OID, num);

            stringToLog(tr("Changing oids.h"));
            writeFile(file_name, oids_file_contents);
        }

        // работа с файлами IDL
        {
            for( const InterfaceData& interface : current_class.interfaces )
            {
                if(interface.definition.isEmpty())
                    continue;

                for( auto type_name : current_class.definitions_of_types )
                    data_file_contents.replace(type_name,"");

                cls_file_contents.replace(interface.definition,"");
            }
            cls_file_contents.replace(current_class.idl_definition,"");

            stringToLog(tr("Changing *.idl with class description"));
            writeFile(m_CLSID_filename, cls_file_contents);

            stringToLog(tr("Changing *Data.idl with data description"));
            writeFile(m_data_IDL_filename,data_file_contents);
        }

        stringToLog( tr("Changing uses in solution. (CComPtr(CComQIPtr) -> COmpPtr, macroses, adding includes where nessesary)") );

        QSet<QString> names_of_files_with_new_includes_for_data;
        bool need_replace_coReport = false;
        bool need_replace_coFill   = false;
        bool need_replace_CLSID    = false;

        for( const InterfaceData& interface : current_class.interfaces )
        {
            if(interface.definition.isEmpty())
            {
                if( interface.name == "IOmpReport" )
                    need_replace_coReport = true;

                if( interface.name == "IFillBoxes" )
                    need_replace_coFill = true;

                if( interface.name == "IOmpReportLoader" || interface.name == "IOmpReportView" )
                    need_replace_CLSID = true;

                continue;
            }
            names_of_files_with_new_includes_for_data.insert(interface.absolute_file_name);
        }

        names_of_files_with_new_includes_for_data.insert(h_file_info.absoluteFilePath());
        names_of_files_with_new_includes_for_data.insert(cpp_file_info.absoluteFilePath());
        names_of_files_with_new_includes_for_data.insert(data_file_info.absoluteFilePath());

        QString path = h_file_info.absolutePath();
        QString folder_name = path.mid(path.lastIndexOf("/") + 1);

        for( const InterfaceData& interface : current_class.interfaces )
        {
            if( interface.definition.isEmpty() &&
                !need_replace_coReport &&
                !need_replace_coFill &&
                !need_replace_CLSID )
            {
                continue;
            }

            QString iname_without_I = interface.name.mid(1);

            QSet<QString> names_of_files_with_new_includes_for_interface;

            if( !interface.definition.isEmpty() )
                stringToLog( tr("Replacing CComPtr(CComQIPtr) -> COmpPtr and coOmp -> ompCreate for ") + interface.name );
            else
                stringToLog( tr("Replacing coFill -> ompFill or coReport -> ompReport or CLSID_* -> OID_* for ") + current_class.idl_name );

            QString search_CComPtr_pattern  ("(CComPtr|CComQIPtr)(?<postfix>.*" + interface.name + "[^\\w].*)"); // FIXME: here is the bug when there are several COmpPtr-s in one row
            QString search_coOmp_012_pattern("coOmp(?<postfix>([^3]).*[^\\w]"  + iname_without_I + "[^\\w].*)");
            QString search_coOmp3_pattern   ("coOmp(?<postfix>3.*[^\\w]I?"     + iname_without_I + "[^\\w].*)");
            QString search_coReport_pattern ("coReport(?<postfix>.*[^\\w]" + current_class.idl_name + "[^\\w].*)");
            QString search_coFill_pattern   ("coFill(?<postfix>.*[^\\w]"   + current_class.idl_name + "[^\\w].*)");
            QString search_CLSID_pattern    ("CLSID_" + current_class.idl_name + "(?<postfix>[^\\w])");

            QDirIterator iter_files_in_solution(solution_dir.absolutePath(), QStringList() << "*.h" << "*.cpp", QDir::Files, QDirIterator::Subdirectories);

            while ( iter_files_in_solution.hasNext() )
            {
                QString file_name = iter_files_in_solution.next();

                if( !interface.definition.isEmpty() )
                {
                    replaceMacrosInFile( file_name, folder_name, interface.header_file_name, search_CComPtr_pattern  , "COmpPtr"  , names_of_files_with_new_includes_for_interface );
                    replaceMacrosInFile( file_name, folder_name, interface.header_file_name, search_coOmp_012_pattern, "ompCreate", names_of_files_with_new_includes_for_interface );
                    replaceMacrosInFile( file_name, folder_name, interface.header_file_name, search_coOmp3_pattern   , "ompCreate", names_of_files_with_new_includes_for_interface );
                }

                if(need_replace_coReport)
                    replaceMacrosInFile( file_name, folder_name, interface.header_file_name, search_coReport_pattern,"ompReport", names_of_files_with_new_includes_for_interface, false);

                if(need_replace_coFill)
                    replaceMacrosInFile( file_name, folder_name, interface.header_file_name, search_coFill_pattern,"ompFill", names_of_files_with_new_includes_for_interface, false);

                if(need_replace_CLSID)
                    replaceMacrosInFile( file_name, folder_name, interface.header_file_name, search_CLSID_pattern, current_class.OID, names_of_files_with_new_includes_for_interface, false);
            }

            if(interface.definition.isEmpty())
            {
                continue;
            }

            for( auto interface_include : names_of_files_with_new_includes_for_interface )
                names_of_files_with_new_includes_for_data.insert( interface_include );

            // TODO: add includes where items of enum are used
            stringToLog( tr("Adding includes where data structures are used for ") + interface.name );

            for( const QString& type : interface.types_in_definition )
            {
                QString search_data_pattern("[\\s|\\(](?<type>" + type + ")[\\s|\\*|&]");
                QDirIterator it_for_data(solution_dir.absolutePath(), QStringList() << "*.h" << "*.cpp", QDir::Files, QDirIterator::Subdirectories);

                while ( it_for_data.hasNext() )
                {
                    QString file_name = it_for_data.next();
                    QString file_contents = readFile(file_name);

                    QRegularExpression expr(search_data_pattern);
                    if( !file_contents.contains(expr) )
                        continue;

                    if( !names_of_files_with_new_includes_for_data.contains(file_name) )
                    {
                        QString include_str = findPlaceForIncludeString(file_contents);
                        file_contents.replace(include_str, include_str + "\n#include \"" + folder_name +"\\" + data_file_info.fileName() + "\"");

                        writeFile(file_name, file_contents);

                        names_of_files_with_new_includes_for_data.insert(file_name);
                    }
                }
            }
        }

        stringToLog("\n");
    }

    stringToLog(tr("=====  COMPLETED!!  ====="));

    stringToLog(tr("\n\n=====  SQL SCRIPTS FOR CONVERSION  =====\n\n"));

    QString common_script_string;

    for( const QString& IDL_class_name : m_selected_classes )
    {
        ComClassData& current_class = m_all_classes_in_IDL[ IDL_class_name ];
        common_script_string.append(current_class.SQL_convert_script);
    }

    stringToLog(common_script_string + "\n\n");
    stringToLog(tr("\nDon't forget add *I.h files to solution and delete *.rgs files for coclasses"));

    return;
}

void MainCalculationThread::replaceMacrosInFile( QString& processing_file_name,
                                                 QString &folder_name,
                                                 const QString &interface_header_file_name,
                                                 const QString& search_pattern,
                                                 const QString &replace_to,
                                                 QSet<QString>& interface_includes,
                                                 bool need_add_include )
{
    QString file_contents = readFile(processing_file_name);

    QRegularExpression search_regex(search_pattern);

    if( file_contents.contains(search_regex) )
    {
        QRegularExpressionMatchIterator iter = search_regex.globalMatch(file_contents);
        while( iter.hasNext() )
        {
            QRegularExpressionMatch match = iter.next();
            file_contents.replace(match.captured(), replace_to + match.captured("postfix"));
        }

        if( need_add_include && !interface_includes.contains(processing_file_name) )
        {
            QString include_str = findPlaceForIncludeString(file_contents);
            file_contents.replace(include_str, include_str + "\n#include \"" + folder_name +"\\" + interface_header_file_name + "\"");
        }

        writeFile(processing_file_name, file_contents);

        if( need_add_include )
            interface_includes.insert(processing_file_name);
    }
}

QString MainCalculationThread::findInterfaceDescription(const QString &file_content, const QString &interface_name) const
{
    QRegularExpression interface_description_reg( "(\\/\\/[^\\/;]*)*\\[([^\\]]*\\]){1}\\s+interface "
                                                  + interface_name
                                                  + "\\s*:\\s*(IUnknown|IDispatch)\\s+\\{([^\\}]*\\};?){1}" );
    QRegularExpressionMatchIterator iter = interface_description_reg.globalMatch(file_content);
    if( iter.hasNext() )
    {
        QRegularExpressionMatch interface_description_match = iter.next();
        QString interface_description = interface_description_match.captured();
        return interface_description;
    }

    return "";
}

bool MainCalculationThread::findIdlDatatypes( QString interface_description, QVector<QString>& type_names) const
{
    cutComInterfaceHead(interface_description);

    QRegularExpression known_datatypes_regex("\\](\\s*)(const)?(\\s*)(?!"
                            "BOOL"
                           "|HWND"
                           "|CHAR"
                           "|long"
                           "|int"
                           "|char"
                           "|double"
                           "|const"
                           "|VARIANT"
                           "|BSTR"
                           "|HRESULT"
                           "|LONG_PTR"
                           "|HMENU)(?<typename>\\w+)");
    QRegularExpressionMatchIterator iter = known_datatypes_regex.globalMatch(interface_description);

    QSet<QString> type_names_set;

    while( iter.hasNext() )
    {
        QRegularExpressionMatch datatype_match = iter.next();
        QString type = datatype_match.captured("typename");
        type_names_set.insert(type);
    }

    for(auto type : type_names_set)
    {
        type_names.push_back(type);
    }

    return true;
}

bool MainCalculationThread::findIdlDatatypesDescription(const QString &file_content, QVector<QString>& typenames, QSet<QString> &datatype_declarations) const
{
    for( const QString& type_name : typenames )
    {
        QString struct_pattern("(\\/\\/[^\\/;]*)*typedef struct tag(\\w+)\\s+\\{([^\\}]*\\}\\s*" + type_name + ";){1}");
        QRegularExpression struct_reg( struct_pattern );
        QRegularExpressionMatchIterator struct_iter = struct_reg.globalMatch(file_content);

        bool cont = false;

        while( struct_iter.hasNext() )
        {
            QRegularExpressionMatch match = struct_iter.next();
            datatype_declarations.insert(match.captured());
            cont = true;
        }

        if(cont)
            continue;

        QString enum_pattern("(\\/\\/[^\\/;]*)*(typedef enum\\s+(\\w+)?\\s*\\{[^\\}]*\\}\\s*" + type_name + ";)");
        QRegularExpression enum_reg( enum_pattern );
        QRegularExpressionMatchIterator enum_iter = enum_reg.globalMatch(file_content);

        while( enum_iter.hasNext() )
        {
            QRegularExpressionMatch match = enum_iter.next();
            datatype_declarations.insert(match.captured());
            cont = true;
        }

        if(cont)
            continue;

        stringToLog(ERROR + tr("Type not found ") + type_name);
        return false;
    }

    return true;
}

void MainCalculationThread::cutComInterfaceHead(QString &desc) const
{
    desc.replace(QRegularExpression("([^;]*\\[[^\\[]*\\s*interface)"),"interface");
}

QString MainCalculationThread::convertInterfaceDescToOmp(const QString &interface_description)
{
    QString result = interface_description;

    QRegularExpression default_value("(?<idl_param>\\[[^\\[]*defaultvalue\\(\\s*(?<defaultval>\\w+)\\s*\\)\\s*\\](?<cpp_param>\\s*\\w+\\s*\\**\\s*\\w+))");
    QRegularExpressionMatchIterator iter = default_value.globalMatch(interface_description);

    while( iter.hasNext() )
    {
        QRegularExpressionMatch match = iter.next();
        QString idl_param = match.captured("idl_param");
        QString cpp_param = match.captured("cpp_param");
        QString value = match.captured("defaultval");
        result.replace( result.indexOf( idl_param ), idl_param.length(), cpp_param + " = " + value );
    }

    cutComInterfaceHead(result);

    result.replace("IUnknown","public IOmpUnknown");
    QRegularExpression rx("\\[([.]*[^\\]]*)\\]\\s*");
    result.replace( rx, "" );

    result.replace( "HRESULT", "virtual HRESULT" );
    result.replace( ");", ") = 0;" );
    result.replace( " {", "{" );
    result.replace( " };", "};" );

    return result;
}

bool MainCalculationThread::convertClassHeaderToOmp(const QFileInfo &header_file_info, ComClassData &class_data, QVector<QString> &converted_methods)
{
    QString header_file_contents = readFile(header_file_info.absoluteFilePath());

    QString include_interfaces_str;
    QString default_omp_interface_str;

    QString not_com_interfaces_definitions_string;
    QString dual_object_interfaces_map;

    for( const InterfaceData& interface : class_data.interfaces )
    {
        if( !interface.header_file_name.isEmpty() )
            include_interfaces_str.append("#include \"" + interface.header_file_name + "\"\n");

        if(interface.is_default && !interface.is_com_interface)
            default_omp_interface_str.append("\nOMP_DEFAULT_INTERFACE( " + interface.name + " );");

        if(interface.is_com_interface)
        {
            if(!class_data.is_dual_object)  // is first
                dual_object_interfaces_map.append("\nOMP__COM_MAP_BEGIN( " + interface.name + " )");
            else
                dual_object_interfaces_map.append("\nOMP__COM_INTERFACE_ENTRY( " + interface.name + " )");

            class_data.is_dual_object = true;
        }
        else
            not_com_interfaces_definitions_string.append("," + interface.definition + interface._names_of_methods);
    }

    if( class_data.is_dual_object )
        dual_object_interfaces_map.append("\nOMP__COM_MAP_END()");

    QRegularExpression rx(".*include.*resource.*");
    header_file_contents.replace( rx, include_interfaces_str );

    QRegularExpression expr("(?<class_desc>class[^\\{]+public(.*)CLSID_" + class_data.idl_name + "([^\\}]+\\})+;)");
    int start_pos = header_file_contents.indexOf(expr);

    QString class_desc = expr.globalMatch(header_file_contents).next().captured("class_desc");
    class_desc = class_desc.left(class_desc.indexOf("};"));
    class_desc.append("};");

    header_file_contents.replace(class_desc,"");

    class_desc.replace("ATL_NO_VTABLE ","");

    rx.setPattern("\n.+CComMultiThreadModel.+");
    class_desc.replace( rx, "");
    rx.setPattern("\n.+public CComCoClass.+");
    class_desc.replace( rx, "");
    rx.setPattern("(\n?)\n.*COM.*");
    class_desc.replace( rx, "");
    rx.setPattern("(\n?)\n.*pUnkMarshaler.*");
    class_desc.replace( rx, "");
    rx.setPattern("(\n?)\n.*DECLARE_GET_CONTROLLING_UNKNOWN.*");
    class_desc.replace( rx, "");
    rx.setPattern("(\n?)\n.*DECLARE_NOT_AGGREGATABLE.*");
    class_desc.replace( rx, "");
    rx.setPattern("DECLARE_CLASSFACTORY_SINGLETON.*");
    class_desc.replace( rx, "Loki::CompileTimeError<( ( false ) != 0 )> Отнаследуйтесь_от_COmpSingleton; ( void )Отнаследуйтесь_от_COmpSingleton;");
    rx.setPattern("(\n?)\n.*DECLARE_PROTECT_FINAL_CONSTRUCT.*");
    class_desc.replace( rx, "");
    rx.setPattern("(\n?)\n.*OMP_UNKNOWN_FUNC_DEF.*");
    class_desc.replace( rx, "");
    rx.setPattern("(\n?)\n.*DECLARE_REGISTRY_RESOURCEID.*");
    class_desc.replace( rx, dual_object_interfaces_map );
    rx.setPattern("(\n?)\n.*HRESULT FinalConstruct([.]*[^\\}]*)\\}");
    class_desc.replace( rx, "");
    rx.setPattern("(\\s|\\n)*.*void FinalRelease([.]*[^\\}]*)\\}(\\s|\\n)*");
    class_desc.replace( rx, "\n  " + default_omp_interface_str + "\n");

    class_desc.replace("IFillBoxes", "IOmpFillBoxes");
    class_desc.replace("IOmpTask", "IRunTreeTask");

    class_desc.replace("IOmpReportLoader", "IOmpRLoader");
    class_desc.replace("IOmpReportView", "IOmpRView");

    class_desc.replace("IOmpReport", "IRunReport");

    class_desc.replace("IOmpRLoader", "IOmpReportLoader");
    class_desc.replace("IOmpRView", "IOmpReportView");

    class_desc.replace("IOmpGroupReport", "IRunGroupReport");

    QRegularExpression reg("(?<all>(?<begin>STDMETHOD\\s*\\(\\s*(?<method_name>\\w+)\\s*\\)).+\\))");
    QRegularExpressionMatchIterator iter = reg.globalMatch(class_desc);

    while( iter.hasNext() )
    {
        QRegularExpressionMatch match = iter.next();

        QString method_name = match.captured("method_name");

        if( !not_com_interfaces_definitions_string.contains(method_name) )
            continue;

        converted_methods.push_back(method_name);

        QString line = match.captured("all");
        class_desc.replace( line, line + " override" );
        class_desc.replace(match.captured("begin"),"HRESULT " + method_name);
    }

    QRegularExpression search_cpp_class_name_regexp("class\\s+(?<cpp_name>\\w+)\\s*:");
    class_data.cpp_lang_name = search_cpp_class_name_regexp.match(class_desc).captured("cpp_name");

    if( isClassSO(class_desc) )
        class_data.SQL_convert_script.append("update obj_managers set CLSID=" + class_data.OID + " where CLSID='" + class_data.CLSID + "';\n");

    header_file_contents.insert(start_pos,class_desc);

    writeFile(header_file_info.absoluteFilePath(), header_file_contents);

    return true;
}

bool MainCalculationThread::convertClassImplementationToOmp(const QFileInfo &implementation_file_info, const ComClassData& class_data, const QVector<QString> &methods_to_convert)
{
    QString cpp_file_contents = readFile(implementation_file_info.absoluteFilePath());

    for( const auto& method_name : methods_to_convert )
        cpp_file_contents.replace(QRegularExpression("(STDMETHODIMP)\\s*" + class_data.cpp_lang_name + "::" + method_name), "HRESULT " + class_data.cpp_lang_name + "::" + method_name);

    writeFile(implementation_file_info.absoluteFilePath(),cpp_file_contents);

    return true;
}

QString MainCalculationThread::findPlaceForIncludeString(const QString &file) const
{
    QString search_any_include_pattern("(#include.*)");
    QString search_any_pragma_pattern("(#pragma.*)");

    QRegularExpression include_expr(search_any_include_pattern);
    QRegularExpressionMatchIterator include_iter = include_expr.globalMatch(file);

    QString result;

    while( include_iter.hasNext() )
    {
        QRegularExpressionMatch match = include_iter.next();
        result = match.captured();
    }

    if( !result.isEmpty() )
        return result;

    QRegularExpression pragma_expr(search_any_pragma_pattern);
    QRegularExpressionMatchIterator pragma_iter = pragma_expr.globalMatch(file);

    while( pragma_iter.hasNext() )
    {
        QRegularExpressionMatch match = pragma_iter.next();
        result = match.captured();
    }

    return result;
}

bool MainCalculationThread::isClassSO(const QString &header) const
{
    return header.contains(QRegularExpression("(ISOLoadedAttrSQLStrBuilder"
                                              "|ISOPredefAttrHandle"
                                              "|ISOUserInterfaceHandle"
                                              "|SOLoadedAttrHandle"
                                              "|ISOTypeReference"
                                              "|ISOTypeCast"
                                              "|ISOTrigger"
                                              "|ISOCalcDataProviderFactory)"));
}

void MainCalculationThread::writeToDebugFile(QString &text) const
{
    writeFile(/*"/outfiles/" +*/ "debug.info", text);
}

bool MainCalculationThread::searchInterfaceInPredefinedMakeSql(InterfaceData &interface, ComClassData &class_data)
{
    if(interface.name == "IBObjManager")
    {
        class_data.SQL_convert_script.append("update businessobj_managers set CLSID=" + class_data.OID + " where UPPER(CLSID)=UPPER('" + class_data.CLSID + "');\n");
        interface.is_com_interface = true;
        return true;
    }
    else if(interface.name == "IComponentGroup")
    {
        class_data.SQL_convert_script.append("update COMPUTE_COMPONENT_GROUP SET DATA=" + class_data.OID + " where UPPER(DATA)=UPPER('" + class_data.CLSID + "');\n");
        interface.is_com_interface = true;
        return true;
    }
    else if(interface.name == "IBObjVerification" ||
            interface.name == "IBObjVerification2")
    {
        class_data.SQL_convert_script.append("update bo_verification_components set CLSID=" + class_data.OID + " where UPPER(CLSID)=UPPER('" + class_data.CLSID + "');\n");
        interface.is_com_interface = true;
        return true;
    }
    else if(interface.name == "IOmpReportView")
    {
        class_data.SQL_convert_script.append("update OMPREPORT_LIST set VIEWER=" + class_data.OID + " where UPPER(VIEWER)=UPPER('" + class_data.CLSID + "');\n");
        interface.is_com_interface = true;
        return true;
    }
    else if(interface.name == "IOmpReportLoader")
    {
        class_data.SQL_convert_script.append("update OMPREPORT_LIST set LOADER=" + class_data.OID + " where UPPER(LOADER)=UPPER('" + class_data.CLSID + "');\n");
        interface.is_com_interface = true;
        return true;
    }
    else if(interface.name == "IOmpContextMenu" ||
            interface.name == "IRegSection" ||
            interface.name == "IBOName" ||
            interface.name == "IOMPBrowser")
    {
        interface.is_com_interface = true;
        return true;
    }
    else if( interface.name == "IFillBoxes" )
    {
        interface.header_file_name = "OmpFillBoxes.h";
        interface._names_of_methods.append("FillBox");
    }
    else if( interface.name == "IOmpTask" )
    {
        interface.header_file_name = "RunTreeTaskI.h";
        interface._names_of_methods.append("Run");
    }
    else if( interface.name == "IOmpReport" )
    {
        interface.header_file_name = "RunReportI.h";
        interface._names_of_methods.append("RunReport");
    }
    else if( interface.name == "IOmpGroupReport" )
    {
        interface.header_file_name = "RunGroupReportI.h";
        interface._names_of_methods.append("RunReport");
    } // new added omp interface must be replaced in class definition at convertClassHeaderToOmp method
    else
        return false;

    interface.is_com_interface = false;
    return true;
}

bool MainCalculationThread::doAskYesOrNo(QString ask_message)
{
    pause();
    emit askUserYesOrNo(ask_message);
    m_sync.lock();
    if(m_pause)
        m_pause_cond.wait(&m_sync);
    m_sync.unlock();

    return m_answer_is_yes;
}

void MainCalculationThread::stringToLog(QString log_string) const
{
    emit appendToLog(log_string);

    qDebug() << log_string;
}

QString MainCalculationThread::readFile(const QString& name_of_file)
{
    QFile file(name_of_file);
    file.open(QIODevice::ReadOnly|QIODevice::Text);
    QTextStream file_stream(&file);
    QString file_contents;
    file_contents.append(file_stream.readAll());
    file.close();

    return file_contents;
}

void MainCalculationThread::writeFile(const QString& name_of_file,const QString& contents) const
{
    QFile file( name_of_file );
    file.open(QIODevice::WriteOnly|QIODevice::Text);
    QTextStream file_stream(&file);
    file_stream << contents;
    file.close();
}
