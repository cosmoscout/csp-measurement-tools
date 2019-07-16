# Measurement tools for CosmoScout VR

A CosmoScout VR plugin with several tools for terrain measurements. This plugin is built as part of CosmoScout's build process. See the [main repository](https://github.com/cosmoscout/cosmoscout-vr) for instructions.

* **Location Flag:** Displays the geographic coordinates and the address of the selected point as accurately as possible.
* **Landing Ellipse:** Puts an ellipse on the body, which is controllable by the center and two points. The center also has a Location Flag.
* **Path Measurement:** Enables the user to place a piecewise linear path on the surface of a body. Information about distance and elevation along the path will be shown.
* **Dip & Strike:** Used to measure the orientation of a surface feature. The dip shows the angle/steepness and the strike the orientation of the surface feature.
* **Polygon:** Measures the area and volume of an arbitrary polygon on surface with a Delaunay-mesh.

## Configuration

This plugin can be enabled with the following configuration in your `settings.json`:

```javascript
{
  ...
  "plugins": {
    ...
    "csp-measurement-tools": {
      "polygon": {
        "heightDiff": <float>,
        "maxAttempt": <int>,
        "maxPoints": <int>,
        "sleekness": <int>
      },
      "ellipse": {
        "numSamples": <int>
      },
      "path": {
        "numSamples": <int>
      }
    }
  }
}
```

## MIT License

Copyright (c) 2019 German Aerospace Center (DLR)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
