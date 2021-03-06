const int MAX_MARCHING_STEPS = 128;
const float MIN_DIST = 0.0;
const float MAX_DIST = 100.0;
const float EPSILON = 0.001;
float MARCHING_STEP = 0.9;

float SDF(vec3 p);
float sceneDist = 0.0;

#include ..\\..\\Shaders\\terrainFunctions.c

#define RENDER_CLOUDS 1
#define RENDER_WATER   1

float waterlevel = 70.0;        // height of the water
float wavegain   = 1.0;       // change to adjust the general water wave level
float large_waveheight = 1.0; // change to adjust the "heavy" waves (set to 0.0 to have a very still ocean :)
float small_waveheight = 1.0; // change to adjust the small waves

vec3 fogcolor    = vec3( 0.5, 0.7, 1.1 );              
vec3 skybottom   = vec3( 0.6, 0.8, 1.2 );
vec3 skytop      = vec3(0.05, 0.2, 0.5);
vec3 reflskycolor= vec3(0.025, 0.10, 0.20);
vec3 watercolor  = vec3(0.2, 0.25, 0.3);

float fog;
float h = 20.0;
float alpha = 0.1;
float asum = 0.0;
float sundot; 

vec3 light       = normalize( vec3(  0.1, 0.25,  0.9 ) );

// random/hash function              
float hash( float n )
{
  return fract(cos(n)*41415.92653);
}


// 3d noise function
float noise( in vec3 x )
{
  vec3 p  = floor(x);
  vec3 f  = smoothstep(0.0, 1.0, fract(x));
  float n = p.x + p.y*57.0 + 113.0*p.z;

  return mix(mix(mix( hash(n+  0.0), hash(n+  1.0),f.x),
    mix( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
    mix(mix( hash(n+113.0), hash(n+114.0),f.x),
    mix( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
}

mat3 m = mat3( 0.00,  1.60,  1.20, -1.60,  0.72, -0.96, -1.20, -0.96,  1.28 );

mat2 m2 = mat2(1.6,-1.2,1.2,1.6);

// Fractional Brownian motion
float fbm( vec3 p )
{
  float f = 0.5000*noise( p ); p = m*p*1.1;
  f += 0.2500*noise( p ); p = m*p*1.2;
  f += 0.1666*noise( p ); p = m*p;
  f += 0.0834*noise( p );
  return f;
}

// this calculates the water as a height of a given position
float water( vec2 p , float time)
{
  float height = waterlevel;

  vec2 shift1 = 0.001*vec2( time*160.0*2.0, time*120.0*2.0 );
  vec2 shift2 = 0.001*vec2( time*190.0*2.0, -time*130.0*2.0 );

  // coarse crossing 'ocean' waves...
  float wave = 0.0;
  wave += sin(p.x*0.021  + shift2.x)*4.5;
  wave += sin(p.x*0.0172+p.y*0.010 + shift2.x*1.121)*4.0;
  wave -= sin(p.x*0.00104+p.y*0.005 + shift2.x*0.121)*4.0;
  // ...added by some smaller faster waves...
  wave += sin(p.x*0.02221+p.y*0.01233+shift2.x*3.437)*5.0;
  wave += sin(p.x*0.03112+p.y*0.01122+shift2.x*4.269)*2.5 ;
  wave *= large_waveheight;
  //wave -= fbm(vec3(p*0.004-shift2*.5,1.0))*small_waveheight*24.;
  // ...added by some distored random waves (which makes the water looks like water :)

  float amp = 6.*small_waveheight;
  shift1 *= .3;
  for (int i=0; i<7; i++)
  {
    wave -= abs(sin((noise(vec3(p*0.01+shift1, 1.0))-.5)*3.14))*amp;
    amp *= .51;
    shift1 *= 1.841;
    p *= m2*0.9331;
  }
  
  height += wave;
  return height;
}

// cloud intersection raycasting
float trace_fog(in vec3 rStart, in vec3 rDirection , float time)
{
#if RENDER_CLOUDS
  // makes the clouds moving...
  vec2 shift = vec2( time*80.0, time*60.0 );
  float sum = 0.0;
  float q2 = 0., q3 = 0.;
  for (int q=0; q<10; q++)
  {
    float c = (q2+350.0-rStart.y) / rDirection.y;// cloud distance
    vec3 cpos = rStart + c*rDirection + vec3(831.0, 321.0+q3-shift.x*0.2, 1330.0+shift.y*3.0); // cloud position
    float alpha = smoothstep(0.5, 1.0, fbm( cpos*0.0015 )); // cloud density
	sum += (1.0-sum)*alpha; // alpha saturation
    if (sum>0.98)
        break;
    q2 += 120.;
    q3 += 0.15;
  }
  
  return clamp( 1.0-sum, 0.0, 1.0 );
#else
  return 1.0;
#endif
}

float SDF(vec3 p, float time) {
  h = p.y - water(p.xz, time);
  return max(1.0,1.0*h);
}

float rayMarch(vec3 eye, vec3 marchingDirection, float start, float end, float time)
{
  float t = start;
  float dist = 0.0;
	
  for( int j=0; j<MAX_MARCHING_STEPS; j++ )
  {
    dist = SDF(eye + t * marchingDirection, time);
    t += MARCHING_STEP*dist;
  }

  sceneDist = t;
  
  return sceneDist;
}

vec3 getColor(vec3 p, vec3 cameraEye, vec3 rayDir, vec2 resolution, vec2 fragCoord, mat4 viewMat, float yDirection, float time)
{
  float sundot = clamp(dot(rayDir,light),0.0,1.0);

  vec3 col;
  float fog=0.0;

  if (h > 10.0)
  {
    // render sky
    float t = pow(1.0-0.7*rayDir.y, 15.0);
    col = 0.8*(skybottom*t + skytop*(1.0-t));
    // sun
    col += 0.47*vec3(1.6,1.4,1.0)*pow( sundot, 350.0 );
    // sun haze
    col += 0.4*vec3(0.8,0.9,1.0)*pow( sundot, 2.0 );
#if RENDER_CLOUDS
    // CLOUDS
    vec2 shift = vec2( time*80.0, time*60.0 );
    vec4 sum = vec4(0,0,0,0); 
    for (int q=1000; q<1100; q++) // 100 layers
    {
      float c = (float(q-1000)*12.0+350.0-cameraEye.y) / rayDir.y; // cloud height
      vec3 cpos = cameraEye + c*rayDir + vec3(831.0, 321.0+float(q-1000)*.15-shift.x*0.2, 1330.0+shift.y*3.0); // cloud position
      float alpha = smoothstep(0.5, 1.0, fbm( cpos*0.0015 ))*.9; // fractal cloud density
      vec3 localcolor = mix(vec3( 1.1, 1.05, 1.0 ), 0.7*vec3( 0.4,0.4,0.3 ), alpha); // density color white->gray
      alpha = (1.0-sum.w)*alpha; // alpha/density saturation (the more a cloud layer's density, the more the higher layers will be hidden)
      sum += vec4(localcolor*alpha, alpha); // sum up weightened color
      
      if (sum.w>0.98)
        break;
    }
    float alpha = smoothstep(0.7, 1.0, sum.w);
    sum.rgb /= sum.w+0.0001;

    // This is an important stuff to darken dense-cloud parts when in front (or near)
    // of the sun (simulates cloud-self shadow)
    sum.rgb -= 0.6*vec3(0.8, 0.75, 0.7)*pow(sundot,13.0)*alpha;
    // This brightens up the low-density parts (edges) of the clouds (simulates light scattering in fog)
    sum.rgb += 0.2*vec3(1.3, 1.2, 1.0)* pow(sundot,5.0)*(1.0-alpha);

    col = mix( col, sum.rgb , sum.w*(1.0-t) );
#endif
  }
  else
  {
#if RENDER_WATER        
    //  render water
    
    vec3 wpos = cameraEye + sceneDist*rayDir; // calculate position where ray meets water

    // calculate water-mirror
    vec2 xdiff = vec2(0.1, 0.0)*wavegain*4.;
    vec2 ydiff = vec2(0.0, 0.1)*wavegain*4.;

    // get the reflected ray direction
    rayDir = reflect(rayDir, normalize(vec3(water(wpos.xz-xdiff, time) - water(wpos.xz+xdiff, time), 1.0, water(wpos.xz-ydiff, time) - water(wpos.xz+ydiff, time))));  
    float refl = 1.0-clamp(dot(rayDir,vec3(0.0, 1.0, 0.0)),0.0,1.0);
  
    float sh = smoothstep(0.2, 1.0, trace_fog(wpos+20.0*rayDir,rayDir, time))*.7+.3;
    // water reflects more the lower the reflecting angle is...
    float wsky   = refl*sh;     // reflecting (sky-color) amount
    float wwater = (1.0-refl)*sh; // water-color amount

    sundot = clamp(dot(rayDir,light),0.0,1.0);

    // watercolor

    col = wsky*reflskycolor; // reflecting sky-color 
    col += wwater*watercolor;
    col += vec3(.003, .005, .005) * (wpos.y-waterlevel+30.);

    // Sun
    float wsunrefl = wsky*(0.5*pow( sundot, 10.0 )+0.25*pow( sundot, 3.5)+.75*pow( sundot, 300.0));
    col += vec3(1.5,1.3,1.0)*wsunrefl; // sun reflection

#endif
    // global depth-fog
    float fo = 1.0-exp(-pow(0.0003*sceneDist, 1.5));
    vec3 fco = fogcolor + 0.6*vec3(0.6,0.5,0.4)*pow( sundot, 4.0 );
    col = mix( col, fco, fo );
  }

  return col;
}