# Note on further VST functino
: presets are sample loads + all control configuration. Change between presets to change between tracks basially

# Note number 1:
in the teensy version, there will of course be no tabs, so there will be a control which would determine on which sample we're acting.


# Note on the port 


The way I see it is it will have one button to trigger each sample (like the midi triggers), it will have one button for switching from series to parralel, it will have one potentiometer per freezer control, and then it will have a button for switching which sample tab we're now configuring, and it will have a number of potentiometers for each "per sample" control.



# Note on effects :

Now, I would like to plan for adding a library of per sample effects. This is sensitive regarding the teensy port because I'm sure that even after the port, this library may be subject to change. This imples that there will not be dedicated buttons for these effects. Which means that we have to delve into a aprt of the UI which will mimic the teensy UI, and make sure that what we do there can be done in teensyduino. 
We will thus define the TEENSY_MENU used through the TEENSY_UI, and we want this to have an equivalent and a display in the VSTs, where we will test things.

The teensy UI is organized into pages : the sample1 page, the sample2 page, the sample3 page, the sample4 page, and the presets page. The page which you're on determines the behavior of four buttons : page circle, param circle, param value, action button. 

The display itself is split in four zone. Assuming that we have a 2 rows, 16 columns (2, 16) LCD screen, zone 1 is (0, [0:7]), zone 2 is (0, 8:15), zone 3 is (1, [0:7]), zone 4 is (1, 8:15). All pages have current preset name in zone 2, and current page name in zone 1


Teesny UI : {   

    Sample1 page : {
        display : {
            - zone 1 : "Sample1"
            - zone 2 : preset name
            - zone 3 : name of current effect on sample 1
            - zone 4 : value of effect
        },
        
        buttons functions : {
            - circle page knob : rotate to the right goes to next page (discrete value know where the range is split into 5 equal parts for the five pages)
            - param circle : changes effect applies on the sample (discrete value know where the range is split into as many equal) part as there currently are effects in the library.
            - param value : displays the main apram value of the effect (for now they will have only one value, like distortion level)
            - action button : nothing
        }
    }
    
    Sample 2 page : same as sample 1 but for sample 2,
    Sample 3 page : same as sample 1 but for sample 3,
    Sample 5 page : same as sample 1 but for sample 4,

    Preset page : {
        hidden variables : {
            selected function : can be save, reload or load other preset,
            destination preset : preset to be loaded if action on destination preset
        },

        display : {
            - zone 1 : Preset name
            - zone 2 : f"moveTo:{destination preset}"
            - zone 3 : "save"
            - zone 4 : "reload"
        },

        buttons functions : {
            - circle page knob : rotate to the right goes to next page (discrete value know where the range is split into 5 equal parts for the five pages)
            - param circle : selects save, reload (discrete value know, one third on save, one third on reload, one third on destination preset)
            - param value : change destination preset, looking in the presets in memory
            - action button : execute function designated by hidden variable, save preset if save is selected, reload current preset if reload is selected, load destination preset if destination preset is selected, with the value corresponding to the destination preset
        }
    }
}


Help me plan the development of this function IN THE VST PLUGIN, that is the VST plugin should emulate this display, with functioning VST buttons (that is the circle page, param circle, param value and action buttons), in a new section of the JUCE plugin called "UI emulation". Do you need some driections or are you able to create this? Remember, we have teensy portability in view but we first want to test everything in vst