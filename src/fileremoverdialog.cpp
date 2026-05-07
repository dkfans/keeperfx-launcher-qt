#include "fileremoverdialog.h"
#include "settings.h"
#include "ui_fileremoverdialog.h"

#include <QFile>
#include <QMessageBox>
#include <QStringListModel>
#include <QCloseEvent>

FileRemoverDialog::FileRemoverDialog(QWidget *parent, QStringList &list)
    : QDialog(parent)
    , ui(new Ui::FileRemoverDialog)
    , list(list)
{
    // Setup the UI
    ui->setupUi(this);

    // Disable resizing and remove maximize button
    setFixedSize(size());
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);

    // Load the list of files
    this->model = new QStringListModel(this);
    this->model->setStringList(list);
    ui->listView->setModel(model);
}

FileRemoverDialog::~FileRemoverDialog()
{
    delete ui;
}

void FileRemoverDialog::on_cancelButton_clicked()
{
    // Call the close() method to trigger the closeEvent
    close();
}

void FileRemoverDialog::closeEvent(QCloseEvent *event)
{
    int result = QMessageBox::question(this,
                                       tr("Confirmation", "MessageBox Title"),
                                       tr("KeeperFX might not function correctly if these files are present.\n\nAre you sure you want to keep them?", "MessageBox Text"));

    if (result == QMessageBox::Yes) {
        event->accept(); // Allow the dialog to close
    } else {
        event->ignore(); // Ignore the close event
    }
}

void FileRemoverDialog::on_removeButton_clicked()
{
    qDebug() << "Removing leftover files";

    // Loop through the list of files
    for (const QString &filePath : std::as_const(list)) {

        // Get the file
        QFile file(QCoreApplication::applicationDirPath() + "/" + filePath);
        if (file.exists()) {

            // Remove the file
            if (!file.remove()) {

                // Something went wrong
                QMessageBox::warning(this, "KeeperFX", tr("Something went wrong while removing leftover files.", "MessageBox Text"));
                return;
            }
        }
    }

    // Success
    QMessageBox::information(this, "KeeperFX", tr("Leftover files have been removed!", "MessageBox Text"));

    // Check if the user wants to do this automatically
    if(ui->checkBoxAutoRemoveLeftoverFiles->isChecked() == true){
        Settings::setLauncherSetting("AUTO_REMOVE_LEFTOVER_FILES", true);
    }

    // Close the dialog
    this->accept();
}
