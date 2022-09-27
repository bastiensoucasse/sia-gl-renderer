#version 410 core

uniform sampler2D colorSampler;
uniform sampler2D normalSampler;
uniform vec2 windowSize;
uniform mat4 invProjMat;
uniform vec4 lightPos;
uniform vec3 lightCol;

out vec4 out_color;

vec3 VSPositionFromDepth(vec2 texCoord, float z)
{
    vec4 positionNDC = vec4(2 * vec3(texCoord, z) - 1, 1.f);
    vec4 positionVS = invProjMat * positionNDC;
    return positionVS.xyz / positionVS.w;
}

vec3 phong(vec3 n, vec3 l, vec3 v, vec3 diffuseColor, vec3 specularColor, float specularCoef, float exponent, vec3 lightColor)
{
    float dotProd = max(0, dot(n, l));
    vec3 diffuse = diffuseColor * dotProd;
    vec3 specular = vec3(0.);

    if (dotProd > 0)
    {
        vec3 h = normalize(l + v);
        specular = specularColor * pow(max(0, dot(h, n)), exponent * 4);
    }

    return 0.1 * diffuseColor + 0.9 * (diffuse + specularCoef * specular) * lightColor;
}

void main()
{
    vec2 texCoord = gl_FragCoord.xy / windowSize;

    vec4 colorTex = texture(colorSampler, texCoord);
    vec4 normalTex = texture(normalSampler, texCoord);

    vec3 vertexV = VSPositionFromDepth(texCoord, normalTex.w);

    vec3 n = normalTex.xyz;
    vec3 l = lightPos.xyz - vertexV;
    vec3 v = -vertexV;
    vec3 diffuseColor = colorTex.rgb;
    vec3 specularColor = vec3(1.0);
    float specularCoef = colorTex.a;
    float exponent = 5;
    vec3 lightColor = lightCol / max(1, dot(l, l));

    out_color = vec4(phong(normalize(n), normalize(l), normalize(v), diffuseColor, specularColor, specularCoef, exponent, lightColor), 1);
}
