#version 130
out mediump vec4 color;
in mediump vec2 texcoord;
uniform sampler2D tex;
uniform vec2 cursorPos;
uniform vec2 windowSize;
uniform float flShadow;
uniform float flRadius;
uniform float flEnabled;
uniform float cameraScale;
uniform vec2 screenshotSize;
uniform float blur_outside_flashlight;
uniform float outside_flashlight_blur_radius;

uniform vec2 bubbleStretch;
uniform float bubbleSqueeze;

float sdfEllipse(vec2 center, float radius, vec2 stretch, float squeeze, vec2 p) {
    vec2 offset = p - center;
    float stretchLen = length(stretch);
    
    if (stretchLen < 0.01) {
        return length(offset) - radius;
    }
    
    vec2 stretchDir = stretch / stretchLen;
    vec2 perpDir = vec2(-stretchDir.y, stretchDir.x);
    
    float alongStretch = dot(offset, stretchDir);
    float alongPerp = dot(offset, perpDir);
    
    float radiusAlong = radius * (1.0 + stretchLen);
    float radiusPerp = radius * (1.0 - squeeze);
    
    radiusAlong = clamp(radiusAlong, radius * 0.5, radius * 2.0);
    radiusPerp = clamp(radiusPerp, radius * 0.3, radius * 1.5);
    
    vec2 q = vec2(alongStretch / radiusAlong, alongPerp / radiusPerp);
    float k = length(q);
    
    return k * min(radiusAlong, radiusPerp) - min(radiusAlong, radiusPerp);
}

vec3 getNormal(float sd, float thickness, float scale) {
    float dx = dFdx(sd) / scale;
    float dy = dFdy(sd) / scale;
    float n_cos = max(thickness + sd, 0.0) / thickness;
    float n_sin = sqrt(max(0.0, 1.0 - n_cos * n_cos));
    return normalize(vec3(dx * n_cos, dy * n_cos, n_sin));
}

float height(float sd, float thickness) {
    if (sd >= 0.0) return 0.0;
    if (sd < -thickness) return thickness;
    float x = thickness + sd;
    return sqrt(thickness * thickness - x * x);
}

vec4 gaussianBlur(sampler2D image, vec2 uv, float radius) {
    if (radius < 0.5) {
        return texture(image, uv);
    }
    vec4 color_sum = vec4(0.0);
    float total_weight = 0.0;
    vec2 texelSize = 1.0 / windowSize;
    int samples = int(clamp(radius * 0.5, 1.0, 8.0));
    float sigma = radius * 0.3;
    for (int x = -samples; x <= samples; x++) {
        for (int y = -samples; y <= samples; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float dist_sq = float(x*x + y*y);
            float weight = exp(-dist_sq / (2.0 * sigma * sigma));
            color_sum += texture(image, uv + offset) * weight;
            total_weight += weight;
        }
    }
    return color_sum / total_weight;
}

void main() {
    vec4 cursor = vec4(cursorPos.x, windowSize.y - cursorPos.y, 0.0, 1.0);
    vec2 fragCoord = gl_FragCoord.xy;
    float scaledRadius = flRadius * cameraScale;
    
    float sd = sdfEllipse(cursor.xy, scaledRadius, bubbleStretch, bubbleSqueeze, fragCoord);
    
    if (flShadow < 0.01) {
        color = texture(tex, texcoord);
        return;
    }
    
    float thicknessExponent = 0.5;
    float thickness = 12.0 * pow(cameraScale, thicknessExponent);
    float refractiveIndex = 1.45;
    float baseHeight = thickness * 6.0;
    
    vec4 outsideTexture;
    if (blur_outside_flashlight > 0.5 && flEnabled > 0.5) {
        outsideTexture = gaussianBlur(tex, texcoord, outside_flashlight_blur_radius);
    } else {
        outsideTexture = texture(tex, texcoord);
    }
    
    float bgAlpha = smoothstep(-2.0, 0.0, sd);
    vec4 bgColor = mix(outsideTexture, vec4(0.0, 0.0, 0.0, 0.0), min(bgAlpha, flShadow));
    
    if (sd >= 0.0) {
        color = bgColor;
        return;
    }
    
    if (flEnabled < 0.5) {
        color = mix(texture(tex, texcoord), bgColor, bgAlpha);
        return;
    }
    
    vec3 normal = getNormal(sd, thickness, cameraScale);
    vec3 incident = vec3(0.0, 0.0, -1.0);
    vec3 refractVec = refract(incident, normal, 1.0 / refractiveIndex);
    float h = height(sd, thickness);
    float refractLength = (h + baseHeight) / max(0.001, dot(vec3(0.0, 0.0, -1.0), refractVec));
    vec2 refractOffset = refractVec.xy * refractLength;
    vec2 texelSize = 1.0 / windowSize;
    vec2 refractedUV = texcoord + refractOffset * texelSize;
    
    vec4 refractColor = texture(tex, refractedUV);
    
    vec3 reflectVec = reflect(incident, normal);
    float c = clamp(abs(reflectVec.x - reflectVec.y), 0.0, 1.0);
    vec4 reflectColor = vec4(c, c, c, 0.0);
    float reflectionFactor = (1.0 - normal.z) * 0.2 * (thickness / 12.0);
    vec4 glassColor = mix(refractColor, reflectColor, reflectionFactor);
    glassColor = clamp(glassColor, 0.0, 1.0);
    bgColor = clamp(bgColor, 0.0, 1.0);
    color = mix(glassColor, bgColor, bgAlpha);
}


// #version 130
// out mediump vec4 color;
// in mediump vec2 texcoord;
// uniform sampler2D tex;
// uniform vec2 cursorPos;
// uniform vec2 windowSize;
// uniform float flShadow;
// uniform float flRadius;
// uniform float flEnabled;
// uniform float cameraScale;
// uniform vec2 screenshotSize;
// uniform float blur_outside_flashlight;
// uniform float outside_flashlight_blur_radius;

// float sdfCircle(vec2 center, float radius, vec2 p) {
//     return length(p - center) - radius;
// }

// vec3 getNormal(float sd, float thickness, float scale) {
//     float dx = dFdx(sd) / scale;
//     float dy = dFdy(sd) / scale;
//     float n_cos = max(thickness + sd, 0.0) / thickness;
//     float n_sin = sqrt(max(0.0, 1.0 - n_cos * n_cos));
//     return normalize(vec3(dx * n_cos, dy * n_cos, n_sin));
// }

// float height(float sd, float thickness) {
//     if (sd >= 0.0) return 0.0;
//     if (sd < -thickness) return thickness;
//     float x = thickness + sd;
//     return sqrt(thickness * thickness - x * x);
// }

// vec4 gaussianBlur(sampler2D image, vec2 uv, float radius) {
//     if (radius < 0.5) {
//         return texture(image, uv);
//     }
//     vec4 color_sum = vec4(0.0);
//     float total_weight = 0.0;
//     vec2 texelSize = 1.0 / windowSize;
//     int samples = int(clamp(radius * 0.5, 1.0, 8.0));
//     float sigma = radius * 0.3;
//     for (int x = -samples; x <= samples; x++) {
//         for (int y = -samples; y <= samples; y++) {
//             vec2 offset = vec2(float(x), float(y)) * texelSize;
//             float dist_sq = float(x*x + y*y);
//             float weight = exp(-dist_sq / (2.0 * sigma * sigma));
//             color_sum += texture(image, uv + offset) * weight;
//             total_weight += weight;
//         }
//     }
//     return color_sum / total_weight;
// }

// void main() {
//     vec4 cursor = vec4(cursorPos.x, windowSize.y - cursorPos.y, 0.0, 1.0);
//     vec2 fragCoord = gl_FragCoord.xy;
//     float scaledRadius = flRadius * cameraScale;
//     float sd = sdfCircle(cursor.xy, scaledRadius, fragCoord);
    
//     if (flShadow < 0.01) {
//         color = texture(tex, texcoord);
//         return;
//     }
    
//     float thicknessExponent = 0.5;
//     float thickness = 12.0 * pow(cameraScale, thicknessExponent);
//     float refractiveIndex = 1.45;
//     float baseHeight = thickness * 6.0;
    
//     // Decide whether to use blurred or clear texture for areas OUTSIDE the flashlight
//     vec4 outsideTexture;
//     if (blur_outside_flashlight > 0.5 && flEnabled > 0.5) {
//         outsideTexture = gaussianBlur(tex, texcoord, outside_flashlight_blur_radius);
//     } else {
//         outsideTexture = texture(tex, texcoord);
//     }
    
//     float bgAlpha = smoothstep(-2.0, 0.0, sd);
//     vec4 bgColor = mix(outsideTexture, vec4(0.0, 0.0, 0.0, 0.0), min(bgAlpha, flShadow));
    
//     if (sd >= 0.0) {
//         color = bgColor;
//         return;
//     }
    
//     if (flEnabled < 0.5) {
//         color = mix(texture(tex, texcoord), bgColor, bgAlpha);
//         return;
//     }
    
//     vec3 normal = getNormal(sd, thickness, cameraScale);
//     vec3 incident = vec3(0.0, 0.0, -1.0);
//     vec3 refractVec = refract(incident, normal, 1.0 / refractiveIndex);
//     float h = height(sd, thickness);
//     float refractLength = (h + baseHeight) / max(0.001, dot(vec3(0.0, 0.0, -1.0), refractVec));
//     vec2 refractOffset = refractVec.xy * refractLength;
//     vec2 texelSize = 1.0 / windowSize;
//     vec2 refractedUV = texcoord + refractOffset * texelSize;
    
//     // INSIDE flashlight: always use clear (non-blurred) texture
//     vec4 refractColor = texture(tex, refractedUV);
    
//     vec3 reflectVec = reflect(incident, normal);
//     float c = clamp(abs(reflectVec.x - reflectVec.y), 0.0, 1.0);
//     vec4 reflectColor = vec4(c, c, c, 0.0);
//     float reflectionFactor = (1.0 - normal.z) * 0.2 * (thickness / 12.0);
//     vec4 glassColor = mix(refractColor, reflectColor, reflectionFactor);
//     // glassColor = mix(glassColor, vec4(0.95, 0.97, 1.0, 1.0), 0.05);
//     glassColor = clamp(glassColor, 0.0, 1.0);
//     bgColor = clamp(bgColor, 0.0, 1.0);
//     color = mix(glassColor, bgColor, bgAlpha);
// }



// #version 130

// out mediump vec4 color;
// in mediump vec2 texcoord;

// uniform sampler2D tex;
// uniform vec2 cursorPos;
// uniform vec2 windowSize;
// uniform float flShadow;
// uniform float flRadius;
// uniform float cameraScale;

// void main()
// {
//     vec4 cursor = vec4(cursorPos.x, windowSize.y - cursorPos.y, 0.0, 1.0);

//     float dist = distance(cursor, gl_FragCoord);
//     float delta = fwidth(dist);
//     float alpha = smoothstep(flRadius * cameraScale - delta, flRadius * cameraScale, dist);

//     color = mix(
//         texture(tex, texcoord), 
//         vec4(0.0, 0.0, 0.0, 0.0),
//         // length(cursor - gl_FragCoord) < (flRadius * cameraScale) ? 0.0 : flShadow);
//         min(alpha, flShadow)
//     );
// }
