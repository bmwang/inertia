// vim: ts=2:sw=2:softtabstop=2
#include "sweep.h"

#include "global.h"
#include "loadimage.h"

#include <fstream>

#define EPSILON (.00000000001)

namespace {
  // Used to advance the rotation minimizing frame forward
  // formula from [wang et al. 07]
  inline vec3 advanceFrame(const vec3 &xi, const vec3 &xi1,
			   const vec3 &ti, const vec3 &si, const vec3 &ri,
			   const vec3 &ti1) {
    vec3 v1 = xi1 - xi;
    double c1 = v1 * v1;
    if (c1 == 0)
      return ri;
    vec3 riL = ri - (2/c1)*(v1*ri)*v1;
    vec3 tiL = ti - (2/c1)*(v1*ti)*v1;
    vec3 v2 = ti1 - tiL;
    double c2 = v2 * v2;
    if (c2 == 0)
      return ri;
    return riL - (2/c2)*(v2*riL)*v2;
  }

  // lerps for all types we want to sample as bsplines
  PathPoint lerp(const PathPoint &a, const PathPoint &b, double t) {
    return PathPoint( (1-t)*a.point + t*b.point, (1-t)*a.azimuth + t*b.azimuth, (1-t)*a.scale+t*b.scale );
  }
  vec2 lerp(const vec2 &a, const vec2 &b, double t) {
    return (1-t)*a + t*b;
  }
  double dist2(const PathPoint &a, const PathPoint &b) {
    return (a.point-b.point).length2();
  }
  double dist2(const vec2 &a, const vec2 &b) {
    return (a-b).length2();
  }

  // get a quoted string, unless there's no quote at the start
  // in that case we just get whatever is there.
  string getQuoted(istream &str) {
    string ret;
    char temp;
    str >> temp;
    if (temp != '"') {
      str.putback(temp);
      str >> ret;
    } else {
      getline(str, ret, '"');
    }
    return ret;
  }

  void getValue(istream &str, int &val) {
    int v;
    if (str >> v)
      val = v;
  }
}


template<typename Pt> 
Pt sampleBSpline(const vector<Pt>& cps, double t, bool closed, int degree)
{
  if (cps.empty()) {
    UCBPrint("sampleBSpline", "need at least one control point");
    return Pt();
  }

  // get into 0,1 range
  if (t > 1.0 || t < 0.0)
    t = fmod(t, 1.0);
  if (t < 0.0)
    t += 1.0;
    
  // adjust degree down as needed to get a curve, if too few control points and not on closed loop
  int numCPs = int(closed ? cps.size() + degree : cps.size());
  if (degree >= numCPs)
    degree = numCPs - 1;

  // rescale t to minSupport,maxSupport range
  double minSupport = degree;
  double maxSupport = numCPs;
  t = (1-t)*minSupport + t*maxSupport;

  // 'recursive' form of b-spline is done iteratively here
  Pt *bases = new Pt[degree+1];
  int k = (int)t;

  for (int i = 0; i <= degree; ++i) {
    bases[i] = cps[(k - degree + i) % cps.size()];
  }

  for (int power = 1; power <= degree; ++power) {
    for (int i = 0; i <= degree - power; ++i) {
      int knot = k - degree + power + i;
      double u_i = (double)knot;
      double u_ipr1 = double(knot + degree - power + 1);
      double a = (t - u_i) / (u_ipr1 - u_i);
      bases[i] = lerp(bases[i], bases[i+1], a);
    }
  }

  Pt result = bases[0];

  delete [] bases;

  return result;
}

// create a polyline that samples the curve (helper for the big render function)
template<typename Pt>
void sampleBSpline(const vector<Pt> &pathPts, vector<Pt> &polyline, int totalSamples, bool closed = true, int degree = 3) {
  if (totalSamples == 0)
    return; // ... no samples is easy!

  Pt lastGood;
  for (int i = 0; i < totalSamples + 3; i++) {
    int loc = i % totalSamples;
    double t = loc / double(totalSamples);
    Pt sp = sampleBSpline(pathPts, t);
    if (!polyline.empty() && dist2(sp, lastGood) < EPSILON) {
      continue; // wait for the samples to get a bit further apart ... !
    } else {
      polyline.push_back(sp);
      lastGood = sp;
    }
  }
}

Sweep::Sweep() : globalTwist(0), globalAzimuth(0), widthRepeats(1) {

  loadTexture("resources/textures/road.png", texture);
  lengthRepeats = 50;

	Generate();
    
}

void Sweep::Generate() {
	// Procedurally generated
	TrackGenerator trkGen;
	pathPts = trkGen.getControlPts();
	
	cscape = new Cityscape(trkGen.getXWidth() + 20, trkGen.getZWidth() + 20, 64);

	renderingCity = false;

}

void Sweep::Regenerate() {
	Cityscape * oldcscape = cscape;
	Generate();
	delete oldcscape;
	clearDisplayList();
}


// sample the curve at a point
PathPoint Sweep::sample(double t) {
  return sampleBSpline(pathPts, t);
}
// get the forward direction
vec3 Sweep::sampleForward(double t, double step) {
  return sample(t+step).point - sample(t).point;
}
// search for a non-zero forward direction
vec3 Sweep::sampleForward_nonzero(double t, double step, int searchdist) {
  int k = 1;
  vec3 dir;
  do {
    dir = sampleForward(t,step*k);
  } while (dir.length2() < EPSILON && k++ < searchdist);
  return dir;
}
vec3 Sweep::sampleUp(double t, double step) {
  t = fmod(t, 1.0);
  if (t < 0.0) t+=1.0;
  // orthonormalize the frame
  vec3 dir = sampleForward_nonzero(t, step);
  double azimuth = sample(t).azimuth;
  vec3 up(0, 1, 0);
  dir.normalize();
  vec3 right = dir ^ up;
  up = right ^ dir;
  up = rotation3D(dir, azimuth) * up;
  up.normalize();
        
  return up;
}

//TODO: efficient-ize this
//gives mat from TBN location of worldLoc to world pos
mat4 Sweep::tbnHomogenizedBasis(double t, vec3 worldLoc, double step) {
  vec3 up = this->sampleUp(t,step);
  vec3 forward = this->sampleForward(t,step);
  up.normalize();
  forward.normalize();

  mat4 basis = mat4(vec4(forward, 0),
		    vec4(up, 0),
		    vec4(up ^ forward,0),
		    vec4(worldLoc,1)).transpose();
  return basis;
}

mat3 Sweep::tbnBasis(double t, double step) {
  vec3 up = this->sampleUp(t,step);
  vec3 forward = this->sampleForward(t,step);
  up.normalize();
  forward.normalize();

  mat3 basis = mat3(forward,
		    up,
		    up ^ forward).transpose();
  return basis;
}

// rotates a vector according to the global azimuth, local azimuth, twist, direction, and location on curve
void Sweep::orientVectorInFrame(const vec3 &dir, double percent, double localAz, vec3 &inFrame) {
  double rot = globalAzimuth + globalTwist * percent + localAz;
  inFrame = rotation3D(dir, rot) * inFrame;
}

vec3 Sweep::getFirstUp() {
  return sampleUp(0);
}

// sweep the cross section along the curve (helper for the big render function)
void Sweep::renderSweep(GeometryShader &shader, vector<PathPoint> &polyline, vector<pair<vec2, double> > &profile, double crossSectionScale) {
  vector<pair<vec2, double> > & crossSection = profile;
  vec3 * stripCurr = new vec3[polyline.size()];
  vec3 * stripPrev = new vec3[polyline.size()];
  
  GLint tangentAttrib = shader.getTangentAttrib();
  GLint bitangentAttrib = shader.getBitangentAttrib();
  
  vector<vec3> forwards;
  vector<vec3> ups;
  vector<vec3> rights;

  // Precompute the coordinate axes for each PathPoint
  for (unsigned int j = 0; j < polyline.size() - 3; j++) {
    double percentageAlongTrack = double(j) / (polyline.size() - 3);
    PathPoint pathPtCurr = polyline[j];

    // Create the coordinate axes
    /*vec3 forward = sampleForward(percentageAlongTrack).normalize();
    vec3 up = sampleUp(percentageAlongTrack);
    vec3 right = (forward ^ up).normalize();
    up = (right ^ forward).normalize();

    right *= crossSectionScale;
    up *= crossSectionScale;

    forwards.push_back(forward);
    ups.push_back(up);
    rights.push_back(right);*/

    mat3 tbn = tbnBasis(percentageAlongTrack).transpose();
    forwards.push_back(tbn[0]);
    ups.push_back(tbn[1] * crossSectionScale);
    rights.push_back(tbn[2] * crossSectionScale);
  }
  
  // FOR loop for going AROUND track
  for (unsigned int i = 0; i <= crossSection.size(); i++) {
    int aroundIndexCurr = (i) % crossSection.size();
    int aroundIndexPrev = (i - 1 + crossSection.size()) % crossSection.size();
    double percentageAroundTrackCurr = i != crossSection.size() ? crossSection[aroundIndexCurr].second : 1.0;
    double percentageAroundTrackPrev = i != 0 ? crossSection[aroundIndexPrev].second : 0.0;
    vec2 csPtCurr = crossSection[aroundIndexCurr].first;
    vec2 csPtPrev = crossSection[aroundIndexPrev].first;
    
    // FOR loop for going ALONG track
    // Populating stripCurr
    for (unsigned int j = 0; j < polyline.size() - 3; j++) {
      vec3 pt = rights[j] * csPtCurr[0] + ups[j] * csPtCurr[1];
      stripCurr[j] = polyline[j].point + pt;
    }
    
    if (i > 0) {
      // FOR loop for going ALONG track
      // Drawing the QUAD_STRIP
      glBegin(GL_QUAD_STRIP);
      for (unsigned int j = 0; j <= (polyline.size() - 3); j++) {
        int alongIndexCurr = (j) % (polyline.size() - 3);
        double percentageAlongTrack = double(j) / (polyline.size() - 3);
      
        PathPoint pathPtCurr = polyline[alongIndexCurr];
      
        vec3 quadBitangent = (stripCurr[alongIndexCurr] - stripPrev[alongIndexCurr]).normalize();
        vec3 quadTangent = (forwards[alongIndexCurr]).normalize();
        vec3 quadNormal = (quadBitangent ^ quadTangent).normalize();

        glNormal3dv(&quadNormal[0]);
        glVertexAttrib3fARB(tangentAttrib, quadTangent[0], quadTangent[1], quadTangent[2]);
        glVertexAttrib3fARB(bitangentAttrib, quadBitangent[0], quadBitangent[1], quadBitangent[2]);

        double texS, texT;
        
        texS = lengthRepeats * percentageAlongTrack;

        texT = percentageAroundTrackPrev;
        glTexCoord2d(texS, texT);
        glVertex3dv(&stripPrev[alongIndexCurr][0]);
        
        texT = percentageAroundTrackCurr;
        glTexCoord2d(texS, texT);
        glVertex3dv(&stripCurr[alongIndexCurr][0]);
      }
      glEnd();
    }
    
    // Advance the strips (by swap)
    vec3* stripTemp = stripPrev;
    stripPrev = stripCurr;
    stripCurr = stripTemp;
  }

  delete [] stripCurr;
  delete [] stripPrev;

  /* // Stuff to be able to look at the raw b-spline
  glBegin(GL_LINE_LOOP);
  glLineWidth(10);
  for (unsigned int i = 0; i < (polyline.size() - 3); i++) {
    double t = double(i) / (polyline.size() - 3);
    vec3 v = sample(t).point;
    glVertex3d(v[0], v[1], v[2]);
  }
  glEnd();*/
}

// the big render function
void Sweep::render(GeometryShader &shader, int pathSamplesPerPt, double crossSectionScale, int xsectSamplesPerPt) {
  shader.set();

  // load textures
  //glActiveTexture(GL_TEXTURE3);
  //glBindTexture(GL_TEXTURE_CUBE_MAP, skyMap);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, heightMap);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, normalMap);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);


  int totalSamples = (int) pathPts.size() * pathSamplesPerPt;

  vector<PathPoint> polyline;
  sampleBSpline(pathPts, polyline, totalSamples);

  vector<pair<vec2, double> > profile;
  profile.push_back(pair<vec2, double>(vec2(0, -2), 0.0));
  profile.push_back(pair<vec2, double>(vec2(7, -2), 0.10));
  profile.push_back(pair<vec2, double>(vec2(7, 0), 0.15));
  profile.push_back(pair<vec2, double>(vec2(6, 0), 0.20));
  profile.push_back(pair<vec2, double>(vec2(6, -1), 0.25));
  profile.push_back(pair<vec2, double>(vec2(-6, -1), 0.75));
  profile.push_back(pair<vec2, double>(vec2(-6, 0), 0.80));
  profile.push_back(pair<vec2, double>(vec2(-7, 0), 0.85));
  profile.push_back(pair<vec2, double>(vec2(-7, -2), 0.90));

  int size = (int) polyline.size();
  if (size <= 1) { // a polyline with only one point is pretty lame!
    cout << "not enough curve to sweep ..." << endl;
    return;
  }

  // Render the track geometry
  renderSweep(shader, polyline, profile, crossSectionScale);

  // Carve away the buildings in the way
  sampleBSpline(pathPts, polyline, totalSamples * 10);
  vector<vec3> carveAway;
  for (unsigned int i = 0 ; i < (polyline.size() - 3); i++) {
    carveAway.push_back(polyline[i].point);
  }
  cscape->carve(carveAway);
}

void Sweep::renderWithDisplayList(GeometryShader &shader, int pathSamplesPerPt, bool renderCity, double crossSectionScale, int xsectSamplesPerPt) {
  if (renderingCity != renderCity) {
    renderingCity = renderCity;
    shaderDL.clear();
  }
  
  int shadeId = shader.getId();

  if (shaderDL.count(shadeId) == 0) {
    GLuint DLid = glGenLists(1);
    glNewList(DLid, GL_COMPILE);
    render(shader, pathSamplesPerPt, crossSectionScale, xsectSamplesPerPt);
    if (renderingCity) {
	    cscape->render();
    }
    glEndList();
    shaderDL[shadeId] = DLid;
  }
  glCallList(shaderDL[shadeId]);
  
}

void Sweep::clearDisplayList() {
  for(map<int, GLuint>::iterator it=shaderDL.begin(); it != shaderDL.end(); it++) {
    glDeleteLists(it->second, 1);
  }
  shaderDL.clear();
}

