# VectorRenderer Module

The VectorRenderer module allows you to display vector drawings from JSON data files in your Minecraft client. This module reads vector stroke data and renders it on screen with customizable positioning and scaling.

## Features

- **JSON Vector Data Support**: Reads vector drawing data from JSON files
- **Auto-Centering**: Automatically centers drawings on screen based on canvas dimensions
- **Position Controls**: X and Y offset sliders to move the drawing around
- **Scaling**: Adjustable scale factor for the entire drawing
- **Background Support**: Optional background display behind the drawing
- **Real-time Reloading**: Can reload data when the module is enabled

## Setup

1. **Place your JSON file**: Copy your vector data JSON file to:
   ```
   C:\Users\[YourUsername]\AppData\Local\Packages\Microsoft.MinecraftUWP_8wekyb3d8bbwe\RoamingState\Solstice\Vector\Vector.json
   ```
   
   The module will automatically create the `Vector` directory if it doesn't exist.

2. **Enable the module**: In the ClickGUI, go to Visual category and enable "VectorRenderer"

3. **Adjust settings**: Use the module settings to position and scale your drawing

## JSON Format

Your JSON file should follow this structure:

```json
{
  "canvasDimensions": {
    "width": 800,
    "height": 600
  },
  "strokes": [
    {
      "color": "#FF0000",
      "size": 5,
      "points": [
        {"x": 100, "y": 100},
        {"x": 150, "y": 150},
        {"x": 200, "y": 100}
      ]
    }
  ]
}
```

### JSON Structure Details

- **canvasDimensions**: Specifies the original canvas size
  - `width`: Canvas width in pixels
  - `height`: Canvas height in pixels

- **strokes**: Array of drawing strokes
  - `color`: Hex color code (e.g., "#FF0000" for red)
  - `size`: Stroke width in pixels
  - `points`: Array of coordinate points
    - `x`: X coordinate relative to canvas
    - `y`: Y coordinate relative to canvas

## Module Settings

- **X Offset**: Horizontal position offset (-1000 to 1000)
- **Y Offset**: Vertical position offset (-1000 to 1000)
- **Scale**: Drawing scale factor (0.1 to 5.0)
- **Auto Center**: Automatically center the drawing on screen
- **Show Background**: Display a background behind the drawing
- **Background Opacity**: Background transparency (0.0 to 1.0)
- **Reload on Enable**: Reload JSON data when module is enabled

## Usage Tips

1. **Positioning**: Use X and Y offset sliders to move your drawing around the screen
2. **Scaling**: Adjust the scale to make your drawing larger or smaller
3. **Auto-center**: Enable auto-center to automatically position based on canvas dimensions
4. **Background**: Enable background for better visibility against game elements
5. **File Updates**: The module will reload the JSON file when enabled if "Reload on Enable" is checked

## Example JSON File

A sample JSON file (`sample_vector_data.json`) is included in the workspace. You can copy this to the Solstice directory and rename it to test the module.

## Troubleshooting

- **No drawing appears**: Check that your JSON file exists and is valid
- **Wrong colors**: Ensure color codes are in hex format (e.g., "#FF0000")
- **Drawing too small/large**: Adjust the scale setting
- **Wrong position**: Use the offset sliders or enable auto-center

## Technical Notes

- The module uses ImGui's drawing functions for rendering
- Colors are converted from hex to ImGui color format
- Strokes are rendered as polylines for smooth curves
- The module automatically handles coordinate transformation from canvas space to screen space 