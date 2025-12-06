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

// SDF for circle with smooth edge
float sdfCircle(vec2 center, float radius, vec2 p) {
    return length(p - center) - radius;
}

// Calculate normal from signed distance field
vec3 getNormal(float sd, float thickness, float scale) {
    float dx = dFdx(sd) / scale;
    float dy = dFdy(sd) / scale;
    
    float n_cos = max(thickness + sd, 0.0) / thickness;
    float n_sin = sqrt(max(0.0, 1.0 - n_cos * n_cos));
    
    return normalize(vec3(dx * n_cos, dy * n_cos, n_sin));
}

// Height of the glass surface
float height(float sd, float thickness) {
    if (sd >= 0.0) return 0.0;
    if (sd < -thickness) return thickness;
    
    float x = thickness + sd;
    return sqrt(thickness * thickness - x * x);
}

void main() {
    vec4 cursor = vec4(cursorPos.x, windowSize.y - cursorPos.y, 0.0, 1.0);
    vec2 fragCoord = gl_FragCoord.xy;
    float scaledRadius = flRadius * cameraScale;
    
    float sd = sdfCircle(cursor.xy, scaledRadius, fragCoord);
    
    if (flShadow < 0.01) {
        color = texture(tex, texcoord);
        return;
    }
    
    // Thickness scaling: at scale 1.0 = 12.0, scales smoothly with zoom
    // The exponent controls how aggressively thickness grows with zoom
    float thicknessExponent = 0.5;  // sqrt-like growth
    float thickness = 12.0 * pow(cameraScale, thicknessExponent);
    
    float refractiveIndex = 1.45;
    float baseHeight = thickness * 6.0;
    
    vec4 bgColor = texture(tex, texcoord);
    float bgAlpha = smoothstep(-2.0, 0.0, sd);
    
    bgColor = mix(bgColor, vec4(0.0, 0.0, 0.0, 0.0), min(bgAlpha, flShadow));
    
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
    
    // Reflection based on ray direction components
    float c = clamp(abs(reflectVec.x - reflectVec.y), 0.0, 1.0);
    vec4 reflectColor = vec4(c, c, c, 0.0);
    
    // Mix refraction and reflection based on viewing angle
    // Scale reflection factor by thickness to keep it proportional
    // Divide by 2.0 to make it more subtle by default
    float reflectionFactor = (1.0 - normal.z) * 0.2 * (thickness / 12.0);
    vec4 glassColor = mix(refractColor, reflectColor, reflectionFactor);
    
    glassColor = mix(glassColor, vec4(0.95, 0.97, 1.0, 1.0), 0.05);
    
    // Clamp colors to prevent overshoots
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
