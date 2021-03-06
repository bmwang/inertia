uniform sampler2D textureSource; // the texture with the scene you want to blur
varying vec2 vTexCoord;
uniform vec2 ScaleU;
 
void main(void)
{
   vec4 sum = vec4(0.0);
 
   // blur in y (vertical)
   // take nine samples, with the distance blurSize between them
   sum += texture2D(textureSource, vec2(vTexCoord.x - 4.0*ScaleU.x, vTexCoord.y - 4.0*ScaleU.y)) * 0.05;
   sum += texture2D(textureSource, vec2(vTexCoord.x - 3.0*ScaleU.x, vTexCoord.y - 3.0*ScaleU.y)) * 0.09;
   sum += texture2D(textureSource, vec2(vTexCoord.x - 2.0*ScaleU.x, vTexCoord.y - 2.0*ScaleU.y)) * 0.12;
   sum += texture2D(textureSource, vec2(vTexCoord.x - ScaleU.x, vTexCoord.y - ScaleU.y)) * 0.15;
   sum += texture2D(textureSource, vec2(vTexCoord.x, vTexCoord.y)) * 0.16;
   sum += texture2D(textureSource, vec2(vTexCoord.x + ScaleU.x, vTexCoord.y + ScaleU.y)) * 0.15;
   sum += texture2D(textureSource, vec2(vTexCoord.x + 2.0*ScaleU.x, vTexCoord.y + 2.0*ScaleU.y)) * 0.12;
   sum += texture2D(textureSource, vec2(vTexCoord.x + 3.0*ScaleU.x, vTexCoord.y + 3.0*ScaleU.y)) * 0.09;
   sum += texture2D(textureSource, vec2(vTexCoord.x + 4.0*ScaleU.x, vTexCoord.y + 4.0*ScaleU.y)) * 0.05;
 
   gl_FragColor = sum;
}
