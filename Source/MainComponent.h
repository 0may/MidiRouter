/*
  ==============================================================================

   This code is based on JUCE's HandlinMidiEventsTutorial code (see below) 

   Copyright (c) 2020 - Oliver Mayer - Academy of Fine Arts Nuremberg

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/


/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2017 - ROLI Ltd.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             HandlingMidiEventsTutorial
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Handles incoming midi events.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
				   juce_audio_processors, juce_audio_utils, juce_core,
				   juce_data_structures, juce_events, juce_graphics,
				   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2017, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once


//==============================================================================
class MainComponent : public Component,
	private MidiInputCallback,
	private MidiKeyboardStateListener
{
public:
	MainComponent()
		: keyboardComponent(keyboardState, MidiKeyboardComponent::horizontalKeyboard),
		startTime(Time::getMillisecondCounterHiRes() * 0.001)
	{
		setOpaque(true);

		addAndMakeVisible(midiInputListLabel);
		midiInputListLabel.setText("MIDI Input:", dontSendNotification);
		midiInputListLabel.attachToComponent(&midiInputList, true);

		addAndMakeVisible(midiInputList);
		midiInputList.setTextWhenNoChoicesAvailable("No MIDI Inputs Enabled");
		auto midiInputs = MidiInput::getAvailableDevices();

		StringArray midiInputNames;
		midiInputNames.add("None");
		for (auto input : midiInputs)
			midiInputNames.add(input.name);

		midiInputList.addItemList(midiInputNames, 1);
		midiInputList.onChange = [this] { setMidiInput(midiInputList.getSelectedItemIndex()); };
		setMidiInput(0);



		addAndMakeVisible(midiOutputListLabel);
		midiOutputListLabel.setText("MIDI Output:", dontSendNotification);
		midiOutputListLabel.attachToComponent(&midiOutputList, true);

		addAndMakeVisible(midiOutputList);
		midiOutputList.setTextWhenNoChoicesAvailable("No MIDI Outputs Enabled");
		auto midiOutputs = MidiOutput::getAvailableDevices();

		StringArray midiOutputNames;
		midiOutputNames.add("None");
		for (auto output : midiOutputs)
			midiOutputNames.add(output.name);

		midiOutputList.addItemList(midiOutputNames, 1);
		midiOutputList.onChange = [this] { setMidiOutput(midiOutputList.getSelectedItemIndex()); };
		setMidiOutput(0);


		addAndMakeVisible(keyboardComponent);
		keyboardState.addListener(this);

		addAndMakeVisible(midiMessagesBox);
		midiMessagesBox.setMultiLine(true);
		midiMessagesBox.setReturnKeyStartsNewLine(true);
		midiMessagesBox.setReadOnly(true);
		midiMessagesBox.setScrollbarsShown(true);
		midiMessagesBox.setCaretVisible(false);
		midiMessagesBox.setPopupMenuEnabled(true);
		midiMessagesBox.setColour(TextEditor::backgroundColourId, Colour(0x32ffffd8));
		midiMessagesBox.setColour(TextEditor::outlineColourId, Colour(0x1c000000));
		midiMessagesBox.setColour(TextEditor::shadowColourId, Colour(0x16000000));

		setSize(600, 400);
	}

	~MainComponent() override
	{
		keyboardState.removeListener(this);
		deviceManager.removeMidiInputDeviceCallback(MidiInput::getAvailableDevices()[midiInputList.getSelectedItemIndex()].identifier, this);
	}

	void paint(Graphics& g) override
	{
		g.fillAll(Colours::black);
	}

	void resized() override
	{
		auto area = getLocalBounds();

		midiInputList.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		midiOutputList.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).reduced(8));
		keyboardComponent.setBounds(area.removeFromTop(80).reduced(8));
		midiMessagesBox.setBounds(area.reduced(8));
	}

private:
	static String getMidiMessageDescription(const MidiMessage& m)
	{
		if (m.isNoteOn())           return "Note on " + MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3);
		if (m.isNoteOff())          return "Note off " + MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3);
		if (m.isProgramChange())    return "Program change " + String(m.getProgramChangeNumber());
		if (m.isPitchWheel())       return "Pitch wheel " + String(m.getPitchWheelValue());
		if (m.isAftertouch())       return "After touch " + MidiMessage::getMidiNoteName(m.getNoteNumber(), true, true, 3) + ": " + String(m.getAfterTouchValue());
		if (m.isChannelPressure())  return "Channel pressure " + String(m.getChannelPressureValue());
		if (m.isAllNotesOff())      return "All notes off";
		if (m.isAllSoundOff())      return "All sound off";
		if (m.isMetaEvent())        return "Meta event";

		if (m.isController())
		{
			String name(MidiMessage::getControllerName(m.getControllerNumber()));

			if (name.isEmpty())
				name = "[" + String(m.getControllerNumber()) + "]";

			return "Controller " + name + ": " + String(m.getControllerValue());
		}

		return String::toHexString(m.getRawData(), m.getRawDataSize());
	}

	void logMessage(const String& m)
	{
		midiMessagesBox.moveCaretToEnd();
		midiMessagesBox.insertTextAtCaret(m + newLine);
	}

	/** Starts listening to a MIDI input device, enabling it if necessary. */
	void setMidiInput(int index)
	{
		auto list = MidiInput::getAvailableDevices();

		if (lastInputIndex > 0)
			deviceManager.removeMidiInputDeviceCallback(list[lastInputIndex-1].identifier, this);

		if (index > 0) {

			auto newInput = list[index-1];

			//logMessage("Input selected: " + newInput.name + "\n");

			if (!deviceManager.isMidiInputDeviceEnabled(newInput.identifier))
				deviceManager.setMidiInputDeviceEnabled(newInput.identifier, true);

			deviceManager.addMidiInputDeviceCallback(newInput.identifier, this);
			
		}
		//else {

		//	logMessage("Input selected: None\n");
		//}

		midiInputList.setSelectedId(index+1, dontSendNotification);
		lastInputIndex = index;
	}

	/** Sets the MIDI output device. */
	void setMidiOutput(int index)
	{

		if (index > 0) {
			auto list = MidiOutput::getAvailableDevices();

			auto newOutput = list[index-1];
		//	logMessage("Output selected: " + newOutput.name + "\n");

			midiOutputDevice = MidiOutput::openDevice(newOutput.identifier);
		}
		else {
			//logMessage("Output selected: None\n");
			midiOutputDevice = 0;
		}

		midiOutputList.setSelectedId(index + 1, dontSendNotification);
		lastOutputIndex = index;
	}


	// These methods handle callbacks from the midi device + on-screen keyboard..
	void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override
	{
		const ScopedValueSetter<bool> scopedInputFlag(isAddingFromMidiInput, true);
		keyboardState.processNextMidiEvent(message);
		postMessageToList(message, source->getName());
	}

	void handleNoteOn(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override
	{
		if (!isAddingFromMidiInput)
		{
			auto m = MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
			m.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
			postMessageToList(m, "On-Screen Keyboard");
		}
	}

	void handleNoteOff(MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/) override
	{
		if (!isAddingFromMidiInput)
		{
			auto m = MidiMessage::noteOff(midiChannel, midiNoteNumber);
			m.setTimeStamp(Time::getMillisecondCounterHiRes() * 0.001);
			postMessageToList(m, "On-Screen Keyboard");
		}
	}

	// This is used to dispach an incoming message to the message thread
	class IncomingMessageCallback : public CallbackMessage
	{
	public:
		IncomingMessageCallback(MainComponent* o, const MidiMessage& m, const String& s)
			: owner(o), message(m), source(s)
		{}

		void messageCallback() override
		{
			if (owner != nullptr)
				owner->addMessageToList(message, source);
		}

		Component::SafePointer<MainComponent> owner;
		MidiMessage message;
		String source;
	};

	void postMessageToList(const MidiMessage& message, const String& source)
	{
		if (midiOutputDevice)
			midiOutputDevice->sendMessageNow(message);
		(new IncomingMessageCallback(this, message, source))->post();
	}

	void addMessageToList(const MidiMessage& message, const String& source)
	{
		auto time = message.getTimeStamp() - startTime;

		auto hours = ((int)(time / 3600.0)) % 24;
		auto minutes = ((int)(time / 60.0)) % 60;
		auto seconds = ((int)time) % 60;
		auto millis = ((int)(time * 1000.0)) % 1000;

		auto timecode = String::formatted("%02d:%02d:%02d.%03d",
			hours,
			minutes,
			seconds,
			millis);

		auto description = getMidiMessageDescription(message);

		String midiMessageString(timecode + "  -  " + description + " (" + source + ")"); // [7]
		logMessage(midiMessageString);
	}

	//==============================================================================
	AudioDeviceManager deviceManager;           // [1]
	ComboBox midiInputList;                     // [2]
	Label midiInputListLabel;
	int lastInputIndex = 0;                     // [3]
	bool isAddingFromMidiInput = false;         // [4]

	MidiKeyboardState keyboardState;            // [5]
	MidiKeyboardComponent keyboardComponent;    // [6]


	ComboBox midiOutputList;                     // [2]
	Label midiOutputListLabel;
	int lastOutputIndex = 0;
	std::unique_ptr<MidiOutput> midiOutputDevice;

	TextEditor midiMessagesBox;
	double startTime;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};