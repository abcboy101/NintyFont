// NintyFont - Nintendo binary font editor
// Copyleft TheDzeraora 2020-2021

// This software is provided under
// the GNU General Public License v3
// See license.txt in the root of the
// source tree for full license conditions

//Source file for CharPropPanel GUI class
#include "charproppanel.h"
#include "controls/controls.h"

namespace NintyFont::GUI
{
    // CharPropPanel::CharPropPanel(FontBase **t_font, QGraphicsScene *t_fontView, ViewWidget *t_view, UnicodeNames *t_unicode)
    //     : QDockWidget()
    // {
    //     //Set the values
    //     font = t_font;
    //     fontView = t_fontView;
    //     view = t_view;
    //     unicode = t_unicode;
    //     props = nullptr;
    //     propList = nullptr;
    //     w = nullptr;
    //     layout = nullptr;
    //     addBtn = nullptr;
    //     deleteBtn = nullptr;
    //     editCallback = new MemberCallback{&editCallbackEvent, this, nullptr};

    //     connect(fontView, &QGraphicsScene::selectionChanged, this, &CharPropPanel::updateOnSelectionChanged);
    //     setWindowTitle("Glyph properties");
    // }

    CharPropPanel::CharPropPanel(GlobalStuffs *t_globals, QWidget *t_parent)
        : QDockWidget(t_parent)
    {
        //Set the values
        // font = t_font;
        // fontView = t_fontView;
        // view = t_view;
        // unicode = t_unicode;
        globals = t_globals;
        props = nullptr;
        propList = nullptr;
        w = nullptr;
        layout = nullptr;
        addBtn = nullptr;
        deleteBtn = nullptr;
        emptyGlyph = new QPixmap(128, 128);
        emptyGlyph->fill(Qt::transparent);

        connect(globals->fontView, &QGraphicsScene::selectionChanged, this, &CharPropPanel::updateOnSelectionChanged);
        setWindowTitle("Glyph properties");
    }

    QFrame *CharPropPanel::createHorizontalLine()
    {
        //Ported from BRFNTify-Next
        QFrame *frame = new QFrame();
        frame->setFrameStyle(QFrame::HLine | QFrame::Sunken);
        return frame;
    }

    void CharPropPanel::updateOnSelectionChanged()
    {
        auto items = globals->fontView->selectedItems();
        if (items.size() == 1)
        {
            propList = ((Glyph *)items[0])->props;
            //Don't lock/unlock controls here cause they do it on their own
            for (auto control = controls.begin(); control != controls.end(); control++)
            {
                (*control)->update();
            }
            deleteBtn->setEnabled(true);
            deleteBtn->blockSignals(false);
            importBtn->setEnabled(true);
            importBtn->blockSignals(false);
            exportBtn->setEnabled(true);
            exportBtn->blockSignals(false);

            glyphPixmapView->setPixmap(((Glyph *)items[0])->pixmap->scaled(128, 128, Qt::KeepAspectRatio, Qt::FastTransformation));
        }
        else
        {
            propList = nullptr;
            lockControls();
            glyphPixmapView->setPixmap(*emptyGlyph);
        }
    }

    void CharPropPanel::updateOnFontLoad()
    {
        //Delete the existing controls and layout
        for (auto control = controls.begin(); control != controls.end(); control++)
        {
            delete *control;
            *control = nullptr;
        }
        controls.clear();
        if (layout != nullptr) delete layout;
        if (w != nullptr) delete w;
        w = nullptr;
        layout = nullptr;

        //Fetch the new list of properties
        if (globals->font == nullptr) return;
        props = globals->font->getGlyphPropertyDescriptors();
        if (props == nullptr) return;

        //Set up the GUI
        w = new QWidget();
        setWidget(w);
        layout = new QVBoxLayout(w);
        auto lyt = new QFormLayout();
        for (auto prop = props->begin(); prop != props->end(); prop++)
        {
            switch ((*prop)->ctrlType)
            {
                case PropertyList::ControlType::None:
                    break;
                case PropertyList::ControlType::SpinBox:
                {
                    //auto lyt = new QFormLayout(); //This is a bodge, deal with it
                    Controls::SpinBox *control = new Controls::SpinBox(&propList, *prop, globals);
                    lyt->addRow(QString::fromStdString((*prop)->name), control); //Bodge cont'd
                    //layout->addLayout(lyt);
                    controls.push_back(control);
                    break;
                }
                case PropertyList::ControlType::Label:
                {
                    //auto lyt = new QFormLayout(); //This is a bodge, deal with it
                    bool isHex = false;
                    if ((*prop)->valueRange.first == 1) isHex = true; //If the min of the range is set to one, then we use treat the label as hex (in case it's a numeric property)
                    Controls::Label *control = new Controls::Label(&propList, *prop, globals, isHex);
                    lyt->addRow(QString::fromStdString((*prop)->name), control); //Bodge cont'd
                    //layout->addLayout(lyt);
                    controls.push_back(control);
                    break;
                }
                case PropertyList::ControlType::CodePointPicker:
                {
                    Controls::CodePointPicker *control = new Controls::CodePointPicker(&propList, *prop, globals);
                    layout->addLayout(control);
                    controls.push_back(control);
                    break;
                }
                default:
                    throw std::runtime_error("This hasn't been implemented yet...");
                    break;
            }
        }
        layout->addLayout(lyt); //This is done so that the code point picker (if there's one) ends up above all other properties, since it has to be inserted differently anyways

        //Add Import and Export buttons, they're always visible no matter the format
        importBtn = new QPushButton("Import", w);
        connect(importBtn, &QPushButton::pressed, this, &CharPropPanel::importEvent);
        exportBtn = new QPushButton("Export", w);
        connect(exportBtn, &QPushButton::pressed, this, &CharPropPanel::exportEvent);
        layout->addWidget(createHorizontalLine());
        layout->addWidget(importBtn);
        layout->addWidget(exportBtn);

        //Create Add and Delete buttons. They always exist, even if not added to the layout
        addBtn = new QPushButton("Add", w);
        connect(addBtn, &QPushButton::pressed, this, &CharPropPanel::addEvent);
        deleteBtn = new QPushButton("Delete", w);
        connect(deleteBtn, &QPushButton::pressed, this, &CharPropPanel::deleteEvent);

        //Add buttons to the layout only if they're needed (as not to give the user a sense of that grayed-out buttons can be turned on somehow)
        if (globals->font->canCreateCopyGlyphs() || globals->font->canDeleteGlyphs()) layout->addWidget(createHorizontalLine());
        if (globals->font->canCreateCopyGlyphs()) layout->addWidget(addBtn);
        if (globals->font->canDeleteGlyphs()) layout->addWidget(deleteBtn);

        glyphPixmapView = new QLabel(w);
        layout->addWidget(createHorizontalLine());
        layout->addWidget(glyphPixmapView);

        layout->addStretch();
        lockControls();
    }

    void CharPropPanel::addEvent()
    {
        Glyph *newglyph = globals->font->createEmptyGlyph();
        globals->fontView->addItem(newglyph);
        globals->font->glyphs.push_back(newglyph);
        globals->fontView->update();
        globals->view->updateLayout();
        globals->font->edited = true;
    }

    void CharPropPanel::deleteEvent()
    {
        auto selectedGlyph = ((Glyph *)globals->fontView->selectedItems()[0]);
        auto index = getIndexOfGlyphInStdVec(globals->font->glyphs, selectedGlyph); //I know it's a bodge...
        globals->fontView->selectedItems().clear();
        globals->fontView->removeItem(selectedGlyph);
        globals->font->glyphs.erase(globals->font->glyphs.begin() + index);
        delete selectedGlyph;
        globals->fontView->update();
        globals->view->updateLayout();
        globals->font->edited = true;
    }

    void CharPropPanel::importEvent()
    {
        auto selectedGlyph = ((Glyph *)globals->fontView->selectedItems()[0]);
        selectedGlyph->importEvent();
        globals->font->edited = true;
    }

    void CharPropPanel::exportEvent()
    {
        auto selectedGlyph = ((Glyph *)globals->fontView->selectedItems()[0]);
        selectedGlyph->exportEvent();
    }

    uint32_t CharPropPanel::getIndexOfGlyphInStdVec(std::vector<Glyph*> vec, Glyph *item)
    {
        bool found = false;
        uint32_t i = 0;
        for (; i < vec.size(); i++)
        {
            if (vec[i] == item)
            {
                found = true;
                break;
            }
        }
        if (found) return i;
        return -1;
    }

    void CharPropPanel::lockControls()
    {
        for (auto control = controls.begin(); control != controls.end(); control++)
        {
            (*control)->lockControl();
        }
        deleteBtn->setEnabled(false);
        deleteBtn->blockSignals(true);
        importBtn->setEnabled(false);
        importBtn->blockSignals(true);
        exportBtn->setEnabled(false);
        exportBtn->blockSignals(true);
    }

    void CharPropPanel::unlockControls()
    {
        for (auto control = controls.begin(); control != controls.end(); control++)
        {
            (*control)->unlockControl();
        };
        deleteBtn->setEnabled(true);
        deleteBtn->blockSignals(false);
        importBtn->setEnabled(true);
        importBtn->blockSignals(false);
        exportBtn->setEnabled(true);
        exportBtn->blockSignals(false);
    }

    CharPropPanel::~CharPropPanel()
    {
        if (layout != nullptr) delete layout;
        if (w != nullptr) delete w;
    }
}

