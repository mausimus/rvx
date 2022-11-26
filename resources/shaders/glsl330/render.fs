#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
void main()
{
    //vec4 texelColor = texture(texture0, fragTexCoord);
    finalColor = /*texelColor + */fragColor;
    if(finalColor.a == 0)
    {
        discard;
    }
}
