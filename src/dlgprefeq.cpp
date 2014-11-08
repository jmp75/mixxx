/***************************************************************************
                          dlgprefeq.cpp  -  description
                             -------------------
    begin                : Thu Jun 7 2007
    copyright            : (C) 2007 by John Sully
    email                : jsully@scs.ryerson.ca
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <QWidget>
#include <QString>
#include <QHBoxLayout>

#include "dlgprefeq.h"
#include "engine/enginefilterbessel4.h"
#include "controlobject.h"
#include "controlobjectslave.h"
#include "util/math.h"
#include "playermanager.h"

#define CONFIG_KEY "[Mixer Profile]"
#define ENABLE_INTERNAL_EQ "EnableEQs"

const int kFrequencyUpperLimit = 20050;
const int kFrequencyLowerLimit = 16;

DlgPrefEQ::DlgPrefEQ(QWidget* pParent, EffectsManager* pEffectsManager,
                     ConfigObject<ConfigValue>* pConfig)
        : DlgPreferencePage(pParent),
          m_COLoFreq(CONFIG_KEY, "LoEQFrequency"),
          m_COHiFreq(CONFIG_KEY, "HiEQFrequency"),
          m_pConfig(pConfig),
          m_lowEqFreq(0.0),
          m_highEqFreq(0.0),
          m_pEffectsManager(pEffectsManager),
          m_inSlotPopulateDeckEffectSelectors(false) {

    // Get the EQ Effect Rack
    m_pDeckEQEffectRack = m_pEffectsManager->getDeckEQEffectRack();
    m_eqRackGroup = QString("[EffectRack%1_EffectUnit%2_Effect1]").
            arg(m_pEffectsManager->getDeckEQEffectRackNumber());

    setupUi(this);

    // Connection
    connect(SliderHiEQ, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateHiEQ()));
    connect(SliderHiEQ, SIGNAL(sliderMoved(int)), this, SLOT(slotUpdateHiEQ()));
    connect(SliderHiEQ, SIGNAL(sliderReleased()), this, SLOT(slotUpdateHiEQ()));

    connect(SliderLoEQ, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateLoEQ()));
    connect(SliderLoEQ, SIGNAL(sliderMoved(int)), this, SLOT(slotUpdateLoEQ()));
    connect(SliderLoEQ, SIGNAL(sliderReleased()), this, SLOT(slotUpdateLoEQ()));

    connect(CheckBoxBypass, SIGNAL(stateChanged(int)), this, SLOT(slotBypass(int)));

    connect(CheckBoxShowAllEffects, SIGNAL(stateChanged(int)),
            this, SLOT(slotPopulateDeckEffectSelectors()));

    // Set to basic view if a previous configuration is missing
    CheckBoxShowAllEffects->setChecked(m_pConfig->getValueString(
            ConfigKey(CONFIG_KEY, "AdvancedView"), QString("no")) == QString("yes"));

    // Add drop down lists for current decks and connect num_decks control
    // to slotAddComboBox
    m_pNumDecks = new ControlObjectSlave("[Master]", "num_decks", this);
    m_pNumDecks->connectValueChanged(SLOT(slotAddComboBox(double)));
    slotAddComboBox(m_pNumDecks->get());

    // Restore the state of Bypass check box
    CheckBoxBypass->setChecked(m_pConfig->getValueString(
            ConfigKey(CONFIG_KEY, ENABLE_INTERNAL_EQ), QString("no")) == QString("no"));
    if (CheckBoxBypass->isChecked()) {
        slotBypass(Qt::Checked);
    } else {
        slotBypass(Qt::Unchecked);
    }

    setUpMasterEQ();


    loadSettings();
    slotUpdate();
    slotApply();
}

DlgPrefEQ::~DlgPrefEQ() {
    qDeleteAll(m_deckEqEffectSelectors);
    m_deckEqEffectSelectors.clear();

    qDeleteAll(m_deckFilterEffectSelectors);
    m_deckFilterEffectSelectors.clear();

    int iNum = m_enableWaveformEqCOs.count();
    for (int i=0; i < iNum; i++) {
        delete (m_enableWaveformEqCOs.takeAt(0)); // Always delete element 0
    }
}

void DlgPrefEQ::slotAddComboBox(double numDecks) {
    int oldDecks = m_deckEqEffectSelectors.size();     
    while (m_deckEqEffectSelectors.size() < static_cast<int>(numDecks)) {
        QHBoxLayout* innerHLayout = new QHBoxLayout();
        QLabel* label = new QLabel(QObject::tr("Deck %1").
                            arg(m_deckEqEffectSelectors.size() + 1), this);

        m_enableWaveformEqCOs.append(
                new ControlObject(ConfigKey(PlayerManager::groupForDeck(
                        m_deckEqEffectSelectors.size()), "enableWaveformEq")));

        // Create the drop down list for EQs
        QComboBox* eqComboBox = new QComboBox(this);
        m_deckEqEffectSelectors.append(eqComboBox);
        connect(eqComboBox, SIGNAL(currentIndexChanged(int)),
                this, SLOT(slotEqEffectChangedOnDeck(int)));

        // Create the drop down list for EQs
        QComboBox* filterComboBox = new QComboBox(this);
        m_deckFilterEffectSelectors.append(filterComboBox);
        connect(filterComboBox, SIGNAL(currentIndexChanged(int)),
                this, SLOT(slotFilterEffectChangedOnDeck(int)));

        // Setup the GUI
        innerHLayout->addWidget(label);
        innerHLayout->addWidget(eqComboBox);
        innerHLayout->addWidget(filterComboBox);
        innerHLayout->addStretch();
        verticalLayout_2->addLayout(innerHLayout);
    }
    slotPopulateDeckEffectSelectors(); 
    for (int i = oldDecks; i < static_cast<int>(numDecks); ++i) {
        // Set the configured effect for box and simpleBox or Bessel8 LV-Mix EQ
        // if none is configured
        QString configuredEffect;
        int selectedEffectIndex;
        configuredEffect = m_pConfig->getValueString(ConfigKey(CONFIG_KEY,
                QString("EffectForDeck%1").arg(i + 1)),
                QString("org.mixxx.effects.bessel8lvmixeq"));
        selectedEffectIndex = m_deckEqEffectSelectors[i]->findData(configuredEffect);
        if (selectedEffectIndex < 0) {
            selectedEffectIndex = m_deckEqEffectSelectors[i]->findData("org.mixxx.effects.bessel8lvmixeq");
        }
        m_deckEqEffectSelectors[i]->setCurrentIndex(selectedEffectIndex);
        m_enableWaveformEqCOs[i]->set(1.0);
    }	
}

void DlgPrefEQ::slotPopulateDeckEffectSelectors() {
    m_inSlotPopulateDeckEffectSelectors = true; // prevents a recursive call

    QList<QPair<QString, QString> > availableEQEffectNames; 
    QList<QPair<QString, QString> > availableFilterEffectNames;
    if (CheckBoxShowAllEffects->isChecked()) {
        m_pConfig->set(ConfigKey(CONFIG_KEY, "AdvancedView"), QString("yes"));
        availableEQEffectNames =
                m_pEffectsManager->getAvailableEffectNames().toList();
        availableFilterEffectNames = availableEQEffectNames;
    } else {
        m_pConfig->set(ConfigKey(CONFIG_KEY, "AdvancedView"), QString("no"));
        availableEQEffectNames =
                m_pEffectsManager->getAvailableMixingEqEffectNames().toList();
        availableFilterEffectNames =
                m_pEffectsManager->getAvailableFilterEffectNames().toList();
    }

    foreach (QComboBox* box, m_deckEqEffectSelectors) {
        // Populate comboboxes with all available effects
        // Save current selection
        QString selectedEffectId = box->itemData(box->currentIndex()).toString();
        QString selectedEffectName = box->itemText(box->currentIndex());
        box->clear();
        int currentIndex = -1;// Nothing selected

        int i;
        for (i = 0; i < availableEQEffectNames.size(); ++i) {
            box->addItem(availableEQEffectNames[i].second);
            box->setItemData(i, QVariant(availableEQEffectNames[i].first));
            if (selectedEffectId == availableEQEffectNames[i].first) {
                currentIndex = i; 
            }
        }
        if (currentIndex < 0 && !selectedEffectName.isEmpty()) {
            // current selection is not part of the new list
            // So we need to add it
            box->addItem(selectedEffectName);
            box->setItemData(i, QVariant(selectedEffectId));
            currentIndex = i;
        }
        box->setCurrentIndex(currentIndex); 
    }

    availableFilterEffectNames.append(QPair<QString,QString>("", tr("None")));

    foreach (QComboBox* box, m_deckFilterEffectSelectors) {
        // Populate comboboxes with all available effects
        // Save current selection
        QString selectedEffectId = box->itemData(box->currentIndex()).toString();
        QString selectedEffectName = box->itemText(box->currentIndex());
        box->clear();
        int currentIndex = -1;// Nothing selected

        int i;
        for (i = 0; i < availableFilterEffectNames.size(); ++i) {
            box->addItem(availableFilterEffectNames[i].second);
            box->setItemData(i, QVariant(availableFilterEffectNames[i].first));
            if (selectedEffectId == availableFilterEffectNames[i].first) {
                currentIndex = i;
            }
        }
        if (currentIndex < 0 && !selectedEffectName.isEmpty()) {
            // current selection is not part of the new list
            // So we need to add it
            box->addItem(selectedEffectName);
            box->setItemData(i, QVariant(selectedEffectId));
            currentIndex = i;
        }
        box->setCurrentIndex(currentIndex);
    }


    m_inSlotPopulateDeckEffectSelectors = false;
}

void DlgPrefEQ::loadSettings() {
    QString highEqCourse = m_pConfig->getValueString(ConfigKey(CONFIG_KEY, "HiEQFrequency"));
    QString highEqPrecise = m_pConfig->getValueString(ConfigKey(CONFIG_KEY, "HiEQFrequencyPrecise"));
    QString lowEqCourse = m_pConfig->getValueString(ConfigKey(CONFIG_KEY, "LoEQFrequency"));
    QString lowEqPrecise = m_pConfig->getValueString(ConfigKey(CONFIG_KEY, "LoEQFrequencyPrecise"));

    double lowEqFreq = 0.0;
    double highEqFreq = 0.0;

    // Precise takes precedence over course.
    lowEqFreq = lowEqCourse.isEmpty() ? lowEqFreq : lowEqCourse.toDouble();
    lowEqFreq = lowEqPrecise.isEmpty() ? lowEqFreq : lowEqPrecise.toDouble();
    highEqFreq = highEqCourse.isEmpty() ? highEqFreq : highEqCourse.toDouble();
    highEqFreq = highEqPrecise.isEmpty() ? highEqFreq : highEqPrecise.toDouble();

    if (lowEqFreq == 0.0 || highEqFreq == 0.0 || lowEqFreq == highEqFreq) {
        setDefaultShelves();
        lowEqFreq = m_pConfig->getValueString(ConfigKey(CONFIG_KEY, "LoEQFrequencyPrecise")).toDouble();
        highEqFreq = m_pConfig->getValueString(ConfigKey(CONFIG_KEY, "HiEQFrequencyPrecise")).toDouble();
    }

    SliderHiEQ->setValue(
        getSliderPosition(highEqFreq,
                          SliderHiEQ->minimum(),
                          SliderHiEQ->maximum()));
    SliderLoEQ->setValue(
        getSliderPosition(lowEqFreq,
                          SliderLoEQ->minimum(),
                          SliderLoEQ->maximum()));

    if (m_pConfig->getValueString(
            ConfigKey(CONFIG_KEY, ENABLE_INTERNAL_EQ), "yes") == QString("yes")) {
        CheckBoxBypass->setChecked(false);
    }
}

void DlgPrefEQ::setDefaultShelves() {
    m_pConfig->set(ConfigKey(CONFIG_KEY, "HiEQFrequency"), ConfigValue(2500));
    m_pConfig->set(ConfigKey(CONFIG_KEY, "LoEQFrequency"), ConfigValue(250));
    m_pConfig->set(ConfigKey(CONFIG_KEY, "HiEQFrequencyPrecise"), ConfigValue(2500.0));
    m_pConfig->set(ConfigKey(CONFIG_KEY, "LoEQFrequencyPrecise"), ConfigValue(250.0));
}

void DlgPrefEQ::slotResetToDefaults() {
    setDefaultShelves();
    loadSettings();
    slotUpdate();
    slotApply();
}

void DlgPrefEQ::slotEqEffectChangedOnDeck(int effectIndex) {
    QComboBox* c = qobject_cast<QComboBox*>(sender());
    // Check if qobject_cast was successful
    if (c && !m_inSlotPopulateDeckEffectSelectors) {
        int deckNumber = m_deckEqEffectSelectors.indexOf(c);
        QString effectId = c->itemData(effectIndex).toString();
        m_pDeckEQEffectRack->loadEffectToChainSlot(deckNumber, 0, effectId);

        // Update the configured effect for the current QComboBox
        m_pConfig->set(ConfigKey(CONFIG_KEY, QString("EffectForDeck%1").
                       arg(deckNumber + 1)), ConfigValue(effectId));

        // This is required to remove a previous selected effect that does not
        // fit to the current ShowAllEffects checkbox
        slotPopulateDeckEffectSelectors();
    }
}

void DlgPrefEQ::slotFilterEffectChangedOnDeck(int effectIndex) {
    QComboBox* c = qobject_cast<QComboBox*>(sender());
    // Check if qobject_cast was successful
    if (c && !m_inSlotPopulateDeckEffectSelectors) {
        int deckNumber = m_deckFilterEffectSelectors.indexOf(c);
        QString effectId = c->itemData(effectIndex).toString();
        m_pDeckEQEffectRack->loadEffectToChainSlot(deckNumber, 1, effectId);

        // Update the configured effect for the current QComboBox
        //m_pConfig->set(ConfigKey(CONFIG_KEY, QString("EffectForDeck%1").
        //               arg(deckNumber + 1)), ConfigValue(effectId));

        // This is required to remove a previous selected effect that does not
        // fit to the current ShowAllEffects checkbox
        slotPopulateDeckEffectSelectors();
    }
}

void DlgPrefEQ::slotUpdateHiEQ() {
    if (SliderHiEQ->value() < SliderLoEQ->value())
    {
        SliderHiEQ->setValue(SliderLoEQ->value());
    }
    m_highEqFreq = getEqFreq(SliderHiEQ->value(),
                             SliderHiEQ->minimum(),
                             SliderHiEQ->maximum());
    validate_levels();
    if (m_highEqFreq < 1000) {
        TextHiEQ->setText( QString("%1 Hz").arg((int)m_highEqFreq));
    } else {
        TextHiEQ->setText( QString("%1 kHz").arg((int)m_highEqFreq / 1000.));
    }
    m_pConfig->set(ConfigKey(CONFIG_KEY, "HiEQFrequency"),
                   ConfigValue(QString::number(static_cast<int>(m_highEqFreq))));
    m_pConfig->set(ConfigKey(CONFIG_KEY, "HiEQFrequencyPrecise"),
                   ConfigValue(QString::number(m_highEqFreq, 'f')));

    slotApply();
}

void DlgPrefEQ::slotUpdateLoEQ() {
    if (SliderLoEQ->value() > SliderHiEQ->value())
    {
        SliderLoEQ->setValue(SliderHiEQ->value());
    }
    m_lowEqFreq = getEqFreq(SliderLoEQ->value(),
                            SliderLoEQ->minimum(),
                            SliderLoEQ->maximum());
    validate_levels();
    if (m_lowEqFreq < 1000) {
        TextLoEQ->setText(QString("%1 Hz").arg((int)m_lowEqFreq));
    } else {
        TextLoEQ->setText(QString("%1 kHz").arg((int)m_lowEqFreq / 1000.));
    }
    m_pConfig->set(ConfigKey(CONFIG_KEY, "LoEQFrequency"),
                   ConfigValue(QString::number(static_cast<int>(m_lowEqFreq))));
    m_pConfig->set(ConfigKey(CONFIG_KEY, "LoEQFrequencyPrecise"),
                   ConfigValue(QString::number(m_lowEqFreq, 'f')));

    slotApply();
}

void DlgPrefEQ::slotUpdateFilter(int value) {
    if (m_pEngineEffectMasterEQ) {
        QSlider* slider = qobject_cast<QSlider*>(sender());
        int index = slider->property("index").toInt();
        EffectsRequest* pRequest = new EffectsRequest();
        pRequest->type = EffectsRequest::SET_PARAMETER_PARAMETERS;
        pRequest->pTargetEffect = m_pEngineEffectMasterEQ;
        pRequest->SetParameterParameters.iParameter = index;
        pRequest->value = value;
        pRequest->minimum = -12;
        pRequest->maximum = 12;
        pRequest->default_value = 0;
        m_pEffectsManager->writeRequest(pRequest);
    }
}

int DlgPrefEQ::getSliderPosition(double eqFreq, int minValue, int maxValue) {
    if (eqFreq >= kFrequencyUpperLimit) {
        return maxValue;
    } else if (eqFreq <= kFrequencyLowerLimit) {
        return minValue;
    }
    double dsliderPos = (eqFreq - kFrequencyLowerLimit) / (kFrequencyUpperLimit-kFrequencyLowerLimit);
    dsliderPos = pow(dsliderPos, 1.0 / 4.0) * (maxValue - minValue) + minValue;
    return dsliderPos;
}

void DlgPrefEQ::slotApply() {
    m_COLoFreq.set(m_lowEqFreq);
    m_COHiFreq.set(m_highEqFreq);
}

void DlgPrefEQ::slotUpdate() {
    slotUpdateLoEQ();
    slotUpdateHiEQ();
}

void DlgPrefEQ::slotBypass(int state) {
    if (state) {
        m_pConfig->set(ConfigKey(CONFIG_KEY, ENABLE_INTERNAL_EQ), QString("no"));
        // Disable effect processing for all decks by setting the appropriate
        // controls to 0 ("[EffectRackX_EffectUnitDeck_Effect1],enable")
        int deck = 1;
        foreach(QComboBox* box, m_deckEqEffectSelectors) {
            ControlObject::set(ConfigKey(m_eqRackGroup.arg(deck), "enabled"), 0);
            m_enableWaveformEqCOs[deck - 1]->set(0);
            deck++;
            box->setEnabled(false);
        }
    } else {
        m_pConfig->set(ConfigKey(CONFIG_KEY, ENABLE_INTERNAL_EQ), QString("yes"));
        // Enable effect processing for all decks by setting the appropriate
        // controls to 1 ("[EffectRackX_EffectUnitDeck_Effect1],enable")
        int deck = 1;
        ControlObjectSlave enableControl;
        foreach(QComboBox* box, m_deckEqEffectSelectors) {
            ControlObject::set(ConfigKey(m_eqRackGroup.arg(deck), "enabled"), 1);
            m_enableWaveformEqCOs[deck - 1]->set(1);
            deck++;
            box->setEnabled(true);
        }
    }

    slotApply();
}

void DlgPrefEQ::setUpMasterEQ() {
    // The Master EQ is the only Effect on the last Effect Rack
    EffectPointer masterEQEffect = m_pEffectsManager->getMasterEQEffectRack()->
            getEffectChainSlot(0)->getEffectSlot(0)->getEffect();
    if (masterEQEffect) {
        m_pEngineEffectMasterEQ = masterEQEffect->getEngineEffect();
    } else {
        m_pEngineEffectMasterEQ = NULL;
    }

    // Create and set up Master EQ's sliders
    for (int i = 0; i < 8; i++) {
        QSlider* slider = new QSlider(this);
        slider->setMinimum(-12);
        slider->setMaximum(12);
        slider->setSingleStep(1);
        slider->setValue(0);
        slider->setMinimumHeight(90);
        // Set the index as a property because we need it inside slotUpdateFilter()
        slider->setProperty("index", QVariant(i));
        slidersGridLayout->addWidget(slider, 0, i);
        m_masterEQSliders.append(slider);
        connect(slider, SIGNAL(sliderMoved(int)), this, SLOT(slotUpdateFilter(int)));
    }

    // Display center frequencies for each filter
    float centerFrequencies[8] = {45, 100, 220, 500, 1100, 2500, 5500, 12000};
    for (unsigned int i = 0; i < 8; i++) {
        QLabel* centerFreqLabel = new QLabel(this);
        QString labelText;
        if (centerFrequencies[i] < 1000) {
            labelText = QString("%1 Hz").arg(centerFrequencies[i]);
        } else {
            labelText = QString("%1 kHz").arg(centerFrequencies[i] / 1000);
        }
        centerFreqLabel->setText(labelText);
        slidersGridLayout->addWidget(centerFreqLabel, 1, i, Qt::AlignCenter);
    }
}

double DlgPrefEQ::getEqFreq(int sliderVal, int minValue, int maxValue) {
    // We're mapping f(x) = x^4 onto the range kFrequencyLowerLimit,
    // kFrequencyUpperLimit with x [minValue, maxValue]. First translate x into
    // [0.0, 1.0], raise it to the 4th power, and then scale the result from
    // [0.0, 1.0] to [kFrequencyLowerLimit, kFrequencyUpperLimit].
    double normValue = static_cast<double>(sliderVal - minValue) /
            (maxValue - minValue);
    // Use a non-linear mapping between slider and frequency.
    normValue = normValue * normValue * normValue * normValue;
    double result = normValue * (kFrequencyUpperLimit - kFrequencyLowerLimit) +
            kFrequencyLowerLimit;
    return result;
}

void DlgPrefEQ::validate_levels() {
    m_highEqFreq = math_clamp<double>(m_highEqFreq, kFrequencyLowerLimit,
                                      kFrequencyUpperLimit);
    m_lowEqFreq = math_clamp<double>(m_lowEqFreq, kFrequencyLowerLimit,
                                     kFrequencyUpperLimit);
    if (m_lowEqFreq == m_highEqFreq) {
        if (m_lowEqFreq == kFrequencyLowerLimit) {
            ++m_highEqFreq;
        } else if (m_highEqFreq == kFrequencyUpperLimit) {
            --m_lowEqFreq;
        } else {
            ++m_highEqFreq;
        }
    }
}
