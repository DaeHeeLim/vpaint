// This file is part of VPaint, a vector graphics editor.
//
// Copyright (C) 2016 Boris Dalstein <dalboris@gmail.com>
//
// The content of this file is MIT licensed. See COPYING.MIT, or this link:
//   http://opensource.org/licenses/MIT

#include "BackgroundWidget.h"
#include "Background.h"
#include "ColorSelector.h"
#include "../Global.h" // XXX This is for documentDir()

#include <QFormLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>

namespace
{
// Validator to test whether the entered url is valid
class ImageUrlValidator: public QValidator
{
public:
    ImageUrlValidator(QObject * parent = 0) :
        QValidator(parent)
    {
    }

    State validate(QString & input, int & /*pos*/) const
    {
        // Check that there no more than one wildcard, and
        // no slash after wildcards

        int wildcardCount = 0;
        for (int i=0; i<input.size(); ++i)
        {
            if (input[i] == '*')
            {
                if(wildcardCount == 0)
                    wildcardCount++;
                else
                    return Intermediate;
            }
            else if (input[i] == '/' && wildcardCount > 0)
            {
                return Intermediate;
            }
        }

        return Acceptable;
    }

    void fixup(QString & input) const
    {
        // Get last index of '*' or '/' (return -1 if not found)
        int j = input.lastIndexOf('*');
        int k = input.lastIndexOf('/');

        // Remove '*' if followed by '/'
        if (k > j)
            j = -1;

        // Only keep wildcard at index j
        QString res;
        for (int i=0; i<input.size(); ++i)
            if (input[i] != '*' || i==j)
                res.append(input[i]);

        // Set result
        input = res;
    }
};
}

BackgroundWidget::BackgroundWidget(QWidget * parent) :
    QWidget(parent),
    background_(0),
    isUpdatingFromBackground_(false),
    isBeingEdited_(false)
{
    // Layout
    QFormLayout * layout = new QFormLayout();
    setLayout(layout);

    // Color
    colorSelector_ = new ColorSelector(Qt::white);
    colorSelector_->setToolTip(tr("Set background color"));
    colorSelector_->setStatusTip(tr("Set background color, possibly transparent."));
    layout->addRow(tr("Color:"), colorSelector_);
    connect(colorSelector_, SIGNAL(colorChanged(Color)),
            this, SLOT(processColorSelectorColorChanged_(Color)));

    // Images
    imageLineEdit_ = new QLineEdit();
    imageLineEdit_->setValidator(new ImageUrlValidator(imageLineEdit_));
    imageLineEdit_->setToolTip(tr("Set background image(s) url\n\n"
                                  "Example 1: 'image.png' for the same image at all frames\n"
                                  "Example 2: 'image*.png' for 'image2.png' on frame 2, etc."));
    imageLineEdit_->setStatusTip(tr("Set background image(s) url. For example, set "
                                    "'image.png' for a fixed image shared across all frames, "
                                    ", or set 'image*.png' for 'image1.png' at frame 1, "
                                    "'image2.png' at frame 2, etc. Paths must be relative to "
                                    "where the vec file is saved."));
    imageBrowseButton_ = new QPushButton("...");
    imageBrowseButton_->setToolTip(tr("Browse for background image(s)"));
    imageBrowseButton_->setStatusTip(tr("Browse for background image(s). Select two or more files, "
                                        "and a pattern of the form 'image*.png' will be automatically "
                                        "detected, loading all images matching patterns even if not selected."));
    imageBrowseButton_->setMaximumWidth(30);
    imageRefreshButton_ = new QPushButton(QIcon(":/images/refresh.png"), tr(""));
    imageRefreshButton_->setToolTip(tr("Reload background image(s)"));
    imageRefreshButton_->setStatusTip(tr("Reload background image(s) to reflect changes on disk."));
    imageRefreshButton_->setMaximumWidth(30);
    QHBoxLayout * imagesLayout = new QHBoxLayout();
    imagesLayout->setSpacing(0);
    imagesLayout->addWidget(imageLineEdit_);
    imagesLayout->addWidget(imageBrowseButton_);
    imagesLayout->addWidget(imageRefreshButton_);
    layout->addRow(tr("Image(s):"), imagesLayout);
    connect(imageLineEdit_, SIGNAL(editingFinished()),
            this, SLOT(processImageLineEditEditingFinished_()));
    connect(imageBrowseButton_, SIGNAL(clicked(bool)),
            this, SLOT(processImageBrowseButtonClicked_()));
    connect(imageRefreshButton_, SIGNAL(clicked(bool)),
            this, SLOT(processImageRefreshButtonClicked_()));

    // Position
    leftSpinBox_ = new QDoubleSpinBox();
    leftSpinBox_->setToolTip(tr("X coordinate of top-left corner of background image(s)"));
    leftSpinBox_->setStatusTip(tr("Set the X coordinate of the position of the top-left corner of background image(s)."));
    leftSpinBox_->setMaximumWidth(80);
    leftSpinBox_->setMinimum(-1e6);
    leftSpinBox_->setMaximum(1e6);
    topSpinBox_ = new QDoubleSpinBox();
    topSpinBox_->setToolTip(tr("Y coordinate of top-left corner of background image(s)"));
    topSpinBox_->setStatusTip(tr("Set the Y coordinate of the position of the top-left corner of background image(s)."));
    topSpinBox_->setMaximumWidth(80);
    topSpinBox_->setMinimum(-1e6);
    topSpinBox_->setMaximum(1e6);
    QHBoxLayout * positionLayout = new QHBoxLayout();
    positionLayout->addWidget(leftSpinBox_);
    positionLayout->addWidget(topSpinBox_);
    layout->addRow(tr("Position:"), positionLayout);
    connect(leftSpinBox_, SIGNAL(valueChanged(double)),
            this, SLOT(processLeftSpinBoxValueChanged_(double)));
    connect(topSpinBox_, SIGNAL(valueChanged(double)),
            this, SLOT(processTopSpinBoxValueChanged_(double)));
    connect(leftSpinBox_, SIGNAL(editingFinished()),
            this, SLOT(processLeftSpinBoxEditingFinished_()));
    connect(topSpinBox_, SIGNAL(editingFinished()),
            this, SLOT(processTopSpinBoxEditingFinished_()));

    // Size
    sizeComboBox_ = new QComboBox();
    sizeComboBox_->setToolTip(tr("Set size of background image(s)"));
    sizeComboBox_->setStatusTip(tr("Set the size of background image(s)."));
    sizeComboBox_->addItem(tr("Fit to canvas"));
    sizeComboBox_->addItem(tr("Manual"));
    sizeComboBox_->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred));
    widthSpinBox_ = new QDoubleSpinBox();
    widthSpinBox_->setToolTip(tr("Width of background image(s)"));
    widthSpinBox_->setStatusTip(tr("Set width of background image(s)."));
    widthSpinBox_->setMaximumWidth(80);
    widthSpinBox_->setMinimum(-1e6);
    widthSpinBox_->setMaximum(1e6);
    widthSpinBox_->setValue(1280);
    heightSpinBox_ = new QDoubleSpinBox();
    heightSpinBox_->setToolTip(tr("Height of background image(s)"));
    heightSpinBox_->setStatusTip(tr("Set height of background image(s)."));
    heightSpinBox_->setMaximumWidth(80);
    heightSpinBox_->setMinimum(-1e6);
    heightSpinBox_->setMaximum(1e6);
    heightSpinBox_->setValue(720);
    QGridLayout * sizeLayout = new QGridLayout();
    sizeLayout->addWidget(sizeComboBox_, 0, 0, 1, 2);
    sizeLayout->addWidget(widthSpinBox_, 1, 0);
    sizeLayout->addWidget(heightSpinBox_, 1, 1);
    layout->addRow(tr("Size:"), sizeLayout);
    connect(sizeComboBox_, SIGNAL(currentIndexChanged(int)),
            this, SLOT(processSizeComboBoxCurrentIndexChanged_(int)));
    connect(widthSpinBox_, SIGNAL(valueChanged(double)),
            this, SLOT(processWidthSpinBoxValueChanged_(double)));
    connect(heightSpinBox_, SIGNAL(valueChanged(double)),
            this, SLOT(processHeightSpinBoxValueChanged_(double)));
    connect(widthSpinBox_, SIGNAL(editingFinished()),
            this, SLOT(processWidthSpinBoxEditingFinished_()));
    connect(heightSpinBox_, SIGNAL(editingFinished()),
            this, SLOT(processHeightSpinBoxEditingFinished_()));

    // Repeat
    repeatComboBox_ = new QComboBox();
    repeatComboBox_->setToolTip(tr("Repeat background image(s)"));
    repeatComboBox_->setStatusTip(tr("Set whether background image(s) should "
                                     "be repeated, either horizontally, vertically, or both"));
    repeatComboBox_->addItem(tr("No"));
    repeatComboBox_->addItem(tr("Horizontally"));
    repeatComboBox_->addItem(tr("Vertically"));
    repeatComboBox_->addItem(tr("Both"));
    repeatComboBox_->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred));
    layout->addRow(tr("Repeat:"), repeatComboBox_);
    connect(repeatComboBox_, SIGNAL(currentIndexChanged(int)),
            this, SLOT(processRepeatComboBoxCurrentIndexChanged_(int)));

    // Opacity
    opacitySpinBox_ = new QDoubleSpinBox();
    opacitySpinBox_->setToolTip(tr("Opacity of background image(s)"));
    opacitySpinBox_->setStatusTip(tr("Set the opacity of background image(s). Note: this does "
                                     "not affect the opacity of the background color (use an alpha "
                                     "value for the color instead)."));
    opacitySpinBox_->setMaximumWidth(80);
    opacitySpinBox_->setMinimum(0);
    opacitySpinBox_->setMaximum(1);
    opacitySpinBox_->setSingleStep(0.1);
    opacitySpinBox_->setValue(1.0);
    layout->addRow(tr("Opacity:"), opacitySpinBox_);
    connect(opacitySpinBox_, SIGNAL(valueChanged(double)),
            this, SLOT(processOpacitySpinBoxValueChanged_(double)));
    connect(opacitySpinBox_, SIGNAL(editingFinished()),
            this, SLOT(processOpacitySpinBoxEditingFinished_()));

    // Hold
    holdCheckBox_ = new QCheckBox();
    holdCheckBox_->setToolTip(tr("Hold background image(s)"));
    holdCheckBox_->setStatusTip(tr("Set whether to hold background image(s). Example: 'image*.png'"
                                   " with only 'image01.png' and 'image03.png' on disk. At "
                                   "frame 2, if hold is checked, 'image01.png' appears. If hold is "
                                   "not checked, no image appears, unless 'image.png' exists in which "
                                   "case it is used as a fallback value."));
    holdCheckBox_->setChecked(true );
    layout->addRow(tr("Hold:"), holdCheckBox_);
    connect(holdCheckBox_, SIGNAL(toggled(bool)),
            this, SLOT(processHoldCheckBoxToggled_(bool)));

    // Set no background
    setBackground(0);
}

void BackgroundWidget::setBackground(Background * background)
{
    // Delete previous connections
    if (background_)
    {
        background_->disconnect(this);
    }

    // Store value
    background_ = background;

    // Disable all widgets if background_ == NULL
    bool areChildrenEnabled = background_ ? true : false;
    QList<QWidget*> children = findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    foreach (QWidget * w, children)
        w->setEnabled(areChildrenEnabled);

    // Set widgets values from background values
    updateFromBackground_();

    // Create connections
    if (background_)
    {
        // XXX We could instead use individual values connections instead,
        // to avoid updating everything each time
        connect(background_, SIGNAL(changed()), this, SLOT(updateFromBackground_()));
    }
}

void BackgroundWidget::updateFromBackground_()
{
    if (background_)
    {
        // Set guard
        isUpdatingFromBackground_ = true;

        // Color
        colorSelector_->setColor(background_->color());

        // Image
        imageLineEdit_->setText(background_->imageUrl());

        // Position
        leftSpinBox_->setValue(background_->position()[0]);
        topSpinBox_->setValue(background_->position()[1]);

        // Size
        sizeComboBox_->setCurrentIndex((int) background_->sizeType());
        widthSpinBox_->setValue(background_->size()[0]);
        heightSpinBox_->setValue(background_->size()[1]);
        switch (background_->sizeType())
        {
        case Background::SizeType::Cover:
            widthSpinBox_->hide();
            heightSpinBox_->hide();
            break;
        case Background::SizeType::Manual:
            widthSpinBox_->show();
            heightSpinBox_->show();
            break;
        }

        // Repeat
        repeatComboBox_->setCurrentIndex((int) background_->repeatType());

        // Opacity
        opacitySpinBox_->setValue(background_->opacity());

        // Hold
        holdCheckBox_->setChecked(background_->hold());

        // Cache value before editing
        if (!isBeingEdited_)
        {
            dataBeforeEditing_ = background_->data();
        }

        // Unset guard
        isUpdatingFromBackground_ = false;
    }
}

Background * BackgroundWidget::background() const
{
    return background_;
}

void BackgroundWidget::processColorSelectorColorChanged_(const Color & newColor)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        background_->setColor(newColor);
        isBeingEdited_ = false;
        emitCheckpoint_();
    }
}

void BackgroundWidget::processImageLineEditEditingFinished_()
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        background_->setImageUrl(imageLineEdit_->text());
        isBeingEdited_ = false;
        emitCheckpoint_();
    }
}

void BackgroundWidget::processImageBrowseButtonClicked_()
{
    // Get filenames
    QDir documentDir = global()->documentDir();
    QStringList filenames = QFileDialog::getOpenFileNames(
                this,
                tr("Select image, or sequence of images, to set as background"),
                documentDir.path(),
                tr("Image files (*.jpg *.png)"));


    // Convert to path relative to current document
    for (int i=0; i<filenames.size(); ++i)
    {
        filenames[i] = documentDir.relativeFilePath(filenames[i]);
    }

    // Detect wildcard
    QString url;
    if (filenames.size() == 0)
    {
        url = QString();
    }
    else if (filenames.size() == 1)
    {
        url = filenames[0];
    }
    else // filenames.size() >= 2
    {
        // Compute largest shared prefix of first two filenames
        const QString & s0 = filenames[0];
        const QString & s1 = filenames[1];
        int prefixLength = 0;
        while (s0.length() > prefixLength &&
               s1.length() > prefixLength &&
               s0[prefixLength] == s1[prefixLength])
        {
            prefixLength++;
        }

        // Chop digits at the end of prefix
        while (prefixLength > 0 &&
               s0[prefixLength-1].isDigit())
        {
            prefixLength--;
        }

        // Chop minus sign, unless all filenames have one, in which case it's
        // probably intented to be a separating dash and not a minus sign
        if (prefixLength > 0 && s0[prefixLength-1] == '-')
        {
            bool theyAllHaveIt = true;
            for (int i=0; i<filenames.size(); ++i)
            {
                if ( (filenames[i].length() < prefixLength) ||
                     (filenames[i][prefixLength-1] != '-' )    )
                {
                    theyAllHaveIt = false;
                    break;
                }
            }

            if (!theyAllHaveIt)
                prefixLength--;
        }

        // Read wildcard of s0
        int s0WildcardLength = 0;
        if (s0.length() == prefixLength)
        {
            // That's weird, but might be the fallback value with
            // a wildcard at the end (i.e., without file extension)
            s0WildcardLength = 0;
        }
        else if (s0[prefixLength] == '-')
        {
            // s0 wildcard is negative
            s0WildcardLength++;
            while (s0.length() > prefixLength+s0WildcardLength &&
                   s0[prefixLength+s0WildcardLength].isDigit())
            {
                s0WildcardLength++;
            }
        }
        else if (s0[prefixLength].isDigit())
        {
            // s0 wildcard is positive
            while (s0.length() > prefixLength+s0WildcardLength &&
                   s0[prefixLength+s0WildcardLength].isDigit())
            {
                s0WildcardLength++;
            }
        }
        else
        {
            // Might be the fallback value
            s0WildcardLength = 0;
        }

        // Deduce prefix and suffix
        int suffixLength = s0.length() - prefixLength - s0WildcardLength;
        QString prefix = s0.left(prefixLength);
        QString suffix = s0.right(suffixLength);

        // Set url
        url = prefix + "*" + suffix;

        // Check for inconsistent names
        QString inconsistentFilenames;
        for (int i=0; i<filenames.size(); ++i)
        {
            bool inconsistent = false;

            if (filenames[i].left(prefixLength)  != prefix ||
                filenames[i].right(suffixLength) != suffix )
            {
                inconsistent = true;
            }
            else
            {
                // Get wildcard
                QString w = filenames[i];
                w.remove(0, prefixLength);
                w.chop(suffixLength);

                if (w.length() == 0)
                {
                    // It's the fallback value: filename[i] == prefix+suffix
                    inconsistent = false;
                }
                else
                {
                    // Try to convert to an int
                    bool ok;
                    w.toInt(&ok);
                    if (!ok)
                    {
                        inconsistent = true;
                    }
                }
            }

            if(inconsistent)
            {
                inconsistentFilenames += filenames[i] + "\n";

            }
        }
        if (inconsistentFilenames.length() > 0)
        {
            // Remove last newline
            inconsistentFilenames.chop(1);

            // issue warning
            QMessageBox::warning(
                        this,
                        tr("Inconsistent file names"),
                        tr("Warning: The selected files don't have a consistent naming scheme. "
                           "The following files do not match \"%1\" and will be ignored:\n%2")
                        .arg(url)
                        .arg(inconsistentFilenames));
        }
    }

    // Set image url
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        background_->setImageUrl(url);
        isBeingEdited_ = false;
        emitCheckpoint_();
    }
}

void BackgroundWidget::processImageRefreshButtonClicked_()
{
    if (background_)
    {
        background_->clearCache();
    }
}

void BackgroundWidget::processLeftSpinBoxValueChanged_(double newLeft)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        double top = background_->position()[1];
        background_->setPosition(Eigen::Vector2d(newLeft, top));
        isBeingEdited_ = false;
    }
}

void BackgroundWidget::processTopSpinBoxValueChanged_(double newTop)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        double left = background_->position()[0];
        background_->setPosition(Eigen::Vector2d(left, newTop));
        isBeingEdited_ = false;
    }
}

void BackgroundWidget::processLeftSpinBoxEditingFinished_()
{
    emitCheckpoint_();
}

void BackgroundWidget::processTopSpinBoxEditingFinished_()
{
    emitCheckpoint_();
}

void BackgroundWidget::processSizeComboBoxCurrentIndexChanged_(int newSizeType)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        background_->setSizeType(static_cast<Background::SizeType>(newSizeType));
        isBeingEdited_ = false;
        emitCheckpoint_();
    }
}

void BackgroundWidget::processWidthSpinBoxValueChanged_(double newWidth)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        double height = background_->size()[1];
        background_->setSize(Eigen::Vector2d(newWidth, height));
        isBeingEdited_ = false;
    }
}

void BackgroundWidget::processHeightSpinBoxValueChanged_(double newHeight)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        double width = background_->size()[0];
        background_->setSize(Eigen::Vector2d(width, newHeight));
        isBeingEdited_ = false;
    }
}

void BackgroundWidget::processWidthSpinBoxEditingFinished_()
{
    emitCheckpoint_();
}

void BackgroundWidget::processHeightSpinBoxEditingFinished_()
{
    emitCheckpoint_();
}

void BackgroundWidget::processRepeatComboBoxCurrentIndexChanged_(int newRepeatType)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        background_->setRepeatType(static_cast<Background::RepeatType>(newRepeatType));
        isBeingEdited_ = false;
        emitCheckpoint_();
    }
}

void BackgroundWidget::processOpacitySpinBoxValueChanged_(double newOpacity)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        background_->setOpacity(newOpacity);
        isBeingEdited_ = false;
    }
}

void BackgroundWidget::processOpacitySpinBoxEditingFinished_()
{
    emitCheckpoint_();
}

void BackgroundWidget::processHoldCheckBoxToggled_(bool newHold)
{
    if (background_ && !isUpdatingFromBackground_)
    {
        isBeingEdited_ = true;
        background_->setHold(newHold);
        isBeingEdited_ = false;
        emitCheckpoint_();
    }
}

void BackgroundWidget::emitCheckpoint_()
{
    if (background_ && (background_->data() != dataBeforeEditing_))
    {
        dataBeforeEditing_ = background_->data();
        background_->emitCheckpoint();
    }
}