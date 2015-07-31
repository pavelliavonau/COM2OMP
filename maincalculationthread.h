#ifndef MAINCALCULATIONTHREAD_H
#define MAINCALCULATIONTHREAD_H

#include "coclassdata.h"

#include <QFileInfo>
#include <QMutex>
#include <QThread>
#include <QVector>
#include <QWaitCondition>
#include <QDir>
#include <QMap>

class QObject;
class QTextEdit;

class MainCalculationThread : public QThread
{
    Q_OBJECT
public:
    MainCalculationThread(QObject* parent);

    void setCalculationData(QVector<ComClassData> allClassesInIdl, QVector<QString> selectedClasses, QString clsIdlFilename, QString dataIdlFilename);

signals:
    void appendToLog(QString) const;
    void askUserYesOrNo(QString);

public slots:
    void answerUserYesOrNoSlot(bool ignore);

    // QThread interface
protected:
    void run() override;

private:
    void resume();
    void pause();

    QString findInterfaceDescription(const QString& file_content, const QString& interface_name) const;
    bool findIdlDatatypes(QString interface_description, QVector<QString> &type_names ) const;
    bool findIdlDatatypesDescription(const QString& file_content, QVector<QString> &typenames, QSet<QString> &datatype_declarations ) const;
    void cutComInterfaceHead(QString& desc) const;
    QString convertInterfaceDescToOmp(const QString& interface_description);
    bool convertClassHeaderToOmp(const QFileInfo& header_file_info, ComClassData& class_data, QVector<QString>& converted_methods);
    bool convertClassImplementationToOmp(const QFileInfo& implementation_file_info, const ComClassData &class_data, const QVector<QString> &methods_to_convert);
    QString findPlaceForIncludeString(const QString& file) const;
    bool isClassSO(const QString& header) const;
    void writeToDebugFile(QString &text)const;
    bool searchInterfaceInPredefinedMakeSql(InterfaceData &interface,  ComClassData &class_data);
    bool doAskYesOrNo(QString ask_message);
    void stringToLog(QString log_string) const;
    QString readFile(const QString& name_of_file);
    void writeFile(const QString &name_of_file, const QString &contents) const;
    void replaceMacrosInFile(QString& processing_file_name,
                              QString& folder_name,
                              const QString& interface_header_file_name,
                              const QString& search_pattern,
                              const QString& replace_to,
                              QSet<QString>& interface_includes,
                              bool need_add_include = true);


    QDir                            m_project_dir;
    QMap< QString, ComClassData >   m_all_classes_in_IDL;
    QVector<QString>                m_selected_classes;
    QString                         m_CLSID_filename;
    QString                         m_data_IDL_filename;
    QString                         m_OID_of_DLL;
    QTextEdit*                      m_log_ptr;

    QMutex              m_sync;
    QWaitCondition      m_pause_cond;

    bool m_pause;
    bool m_answer_is_yes;
    bool m_always_yes;
    bool m_always_yes_asked;
};

#endif // MAINCALCULATIONTHREAD_H
