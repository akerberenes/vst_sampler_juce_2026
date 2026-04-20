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

