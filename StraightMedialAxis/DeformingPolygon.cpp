#include <DeformingPolygon.h>
#include <map>
#include <set>
#include <szMiscOperations.h>
#include <szMexUtility.h>
#include <IntersectionConvexPolygons.h>
#include <DisjointSet.h>
#include <mex.h> //just for printf
#include <stack>

namespace StraightAxis
{

/*
A unit vector from o to x in 2D
*/
CParticleF 
NormalizedDirection(CParticleF& x, CParticleF& o)
{
	float dx = x.m_X - o.m_X;
	float dy = x.m_Y - o.m_Y;
	float len = sqrt(dx*dx + dy*dy);
	if(len > 1.0e-5)
	{
		return CParticleF(dx/len, dy/len);
	}
	else
	{
		return CParticleF(0.0f, 0.0f);
	}
}

/*
A unit vector from o to x in 3D
*/
CParticleF 
NormalizedDirection3D(CParticleF& x, CParticleF& o)
{
	float dx = x.m_X - o.m_X;
	float dy = x.m_Y - o.m_Y;
	float dz = x.m_Z - o.m_Z;
	float len = sqrt(dx*dx + dy*dy + dz*dz);
	if(len > 1.0e-5)
	{
		return CParticleF(dx/len, dy/len, dz/len);
	}
	else
	{
		return CParticleF(0.0f, 0.0f, 0.0f);
	}
}

/*
return a unit vector that bisects the angle formed by three points: a-o-b.
*/
CParticleF
bisector(CParticleF& o, CParticleF& a, CParticleF& b)
{
	CParticleF x = NormalizedDirection(a, o);
	CParticleF y = NormalizedDirection(b, o);
	CParticleF z((x.m_X+y.m_X)/2, (x.m_Y+y.m_Y)/2);
	float vx = z.m_X;
	float vy = z.m_Y;
	float len0 = sqrt(vx*vx + vy*vy);
	if(len0 <= 1.0e-5) //this is a colinear point. 
	{
		float ang = GetVisualDirection(b.m_X, b.m_Y, a.m_X, a.m_Y) - PI/2.0;
		vx = cos(ang);
		vy = sin(ang);
	}
	else
	{
		vx = vx / len0;
		vy = vy / len0;
	}
	CParticleF bs(o.m_X+vx, o.m_Y+vy);
	//test for orientation
	vector<CParticleF> pnts(4);
	pnts[0] = o;
	pnts[1] = a;
	pnts[2] = bs;
	pnts[3] = b;
	if(ClockWise(pnts)<0)
	{
		vx = -vx;
		vy = -vy;
	}
	return CParticleF(vx, vy);
}

float
intersectPlaneAndLine(CParticleF op, CParticleF& z, CParticleF& ol, CParticleF u)
{
	//move the origin to ol.
	op.m_X -= ol.m_X;
	op.m_Y -= ol.m_Y;
	op.m_Z -= ol.m_Z;
	float t = (z.m_X * op.m_X + z.m_Y * op.m_Y + z.m_Z * op.m_Z) / (z.m_X * u.m_X + z.m_Y * u.m_Y + z.m_Z * u.m_Z);
	return t;
}

/*
return true if x is on a line segment between [p, q) or (x0, x) crosses [p, q) where x0 is the original position of x.
*/
bool
overlap(CParticleF& x, CParticleF& p, CParticleF& q, CParticleF& x0, float eps)
{
	float a = p.m_X;
	float b = q.m_X-p.m_X;
	float c = p.m_Y;
	float d = q.m_Y-p.m_Y;
	float det = b*b + d*d;
	if(det < 1.0e-5) //p and q are too close
	{
		return Distance(x, p) < eps;
	}
	float t = (-a*b - c*d + b*x.m_X + d*x.m_Y)/(b*b + d*d);
	CParticleF y(a+b*t, c+d*t); //a point on the line p-q that is closest to x
	if(t >=0 && t<1.0)
	{
		if(Distance(x, y) < eps) return true;
	}

	//check if (x0, x) crossses (p,q)
	pair<float,float> p1 = _IntersectConvexPolygon::intersect(x0, x, p, q);
	if(p1.first > 0 && p1.first <= 1.0 && p1.second>=0 && p1.second<=1.0) 
	{
		return true;
	}
	//check if (x2, x) crossses (p,q)
	//if all failed
	return false;
}

bool
coLinear(CParticleF& a, CParticleF& b, CParticleF& c, float precision = 1.0e-3)
{
	float d = Distance2Line(a, c, b);
	return d < precision;
}

bool
coLinear(CParticleF& a, CParticleF& b, CParticleF& c, CParticleF& d, float precision = 1.0e-3)
{
	float d1 = Distance2Line(a, c, b);
	float d2 = Distance2Line(a, c, d);
	return d1 < precision && d2 < precision;
}

CParticleF
crossProduct(CParticleF& a, CParticleF& b)
{
	return CParticleF(a.m_Y*b.m_Z-a.m_Z*b.m_Y, a.m_Z*b.m_X-a.m_X*b.m_Z, a.m_X*b.m_Y-a.m_Y*b.m_X);
}

/*
Given three points in 3D, define a unit vector perpendicular to the plane going through the three points.
The vector is always upward (i.e. its Z-coordinate is always positive). But this should not really matter...
*/
CParticleF
perp2Plane(CParticleF& a, CParticleF& b, CParticleF& c)
{
	CParticleF u = NormalizedDirection3D(b, a);
	CParticleF v = NormalizedDirection3D(c, a);
	CParticleF z = crossProduct(u, v);
	if(z.m_Z < 0)
	{
		z.m_X = -z.m_X;
		z.m_Y = -z.m_Y;
		z.m_Z = -z.m_Z;
	}
	return z;
}

bool
calculateVelocity(CParticleF& a, CParticleF& o, CParticleF& b, float& vx, float& vy)
{
	CParticleF bs = bisector(o, a, b);
	float ang = GetVisualAngle2(a.m_X, a.m_Y, b.m_X, b.m_Y, o.m_X, o.m_Y);
	float cs = cos((PI - Abs(ang)) / 2.0);
	float len = 1.0f / cs;
	bs.m_X *= len;
	bs.m_Y *= len;
	/*if(ang<0)
	{
		bs.m_X = -bs.m_X;
		bs.m_Y = -bs.m_Y;
	}*/
	vx = bs.m_X;
	vy = bs.m_Y;
	if(Abs(vx)>100000 || Abs(vy)>100000)
		vx += 0;
	return true;
}

bool 
MovingParticle::setVelocity()
{
	float eps = 1.0e-5;
	if(next && prev)
	{
		/*if(Distance(next->p, prev->p) < eps)
		{
			CParticleF d = NormalizedDirection(next->p, p);
			v[0] = d.m_X;
			v[1] = d.m_Y;
		}
		else
		{
			calculateVelocity(prev->p, p, next->p, v[0], v[1]);
		}*/
		calculateVelocity(prev->p, p, next->p, v[0], v[1]);
		return true;
	}
	else
	{
		v[0] = 0; v[1] = 0;
		return false;
	}
}

void
setNeighbors(MovingParticle* p, MovingParticle* prev, MovingParticle* next)
{
	p->prev = prev;
	p->next = next;
	p->prev->next = p;
	p->next->prev = p;
	p->setVelocity();
}

DeformingPolygon::DeformingPolygon(vector<CParticleF>& pnts)
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	float eps = 1.0e-5;
	for(int i=0; i<pnts.size(); ++i)
	{
		MovingParticle* p = factory->makeParticle(pnts[i], Regular);
		particles.push_back(p);
	}
	for(int i=0; i<particles.size(); ++i)
	{
		//particles[i]->p0 = particles[i]->p;
		int j = (i+1) % particles.size();
		particles[i]->next = particles[j];
		int k = (i-1+particles.size()) % particles.size();
		particles[i]->prev = particles[k];
		particles[i]->setVelocity();
	}
}

pair<float,MovingParticle*>
DeformingPolygon::nextEdgeEvent()
{
	float t = std::numeric_limits<float>::infinity();
	pair<float,MovingParticle*> res(t, (MovingParticle*)NULL);
	for(int i=0; i<particles.size(); ++i)
	{
		CParticleF p2 = particles[i]->move(1);
		CParticleF q2 = particles[i]->next->move(1);
		pair<float,float> param = _IntersectConvexPolygon::intersect(particles[i]->p, p2, particles[i]->next->p, q2);
		float t0 = Max(param.first, param.second);
		if(param.first>0 && param.second >0 && t0 < t)
		{
			t = t0;
			res.first = t;
			res.second = particles[i];
		}
	}
	return res;
}

pair<float,MovingParticle*>
DeformingPolygon::nextSplitEvent()
{
	float t = std::numeric_limits<float>::infinity();
	pair<float,MovingParticle*> res(t, (MovingParticle*)NULL);
	for(int i=0; i<particles.size(); ++i)
	{
		//CParticleF bs = bisector(particles[i]->p, particles[i]->prev->p, particles[i]->next->p);
		CParticleF dn = NormalizedDirection(particles[i]->next->p, particles[i]->p);
		CParticleF dp = NormalizedDirection(particles[i]->prev->p, particles[i]->p);
		if(dn.m_X * particles[i]->v[0] + dn.m_Y * particles[i]->v[1] < 0 &&
			dp.m_X * particles[i]->v[0] + dp.m_Y * particles[i]->v[1] < 0) //concave
		{
			CParticleF ol = particles[i]->p;
			CParticleF ul(particles[i]->v[0], particles[i]->v[1], 1.0);
			for(int j=0; j<particles.size(); ++j)
			{
				if(particles[i]==particles[j] || particles[i]->next==particles[j] || particles[i]->prev==particles[j]) continue;
				CParticleF op = particles[j]->p;
				CParticleF oq = particles[j]->next->p;
				CParticleF or(op.m_X + particles[j]->v[0], op.m_Y + particles[j]->v[1], 1.0f);
				CParticleF up = perp2Plane(op, oq, or);
				float t0 = intersectPlaneAndLine(op, up, ol, ul);
				if(t0 > 0 && t0<t) 
				{
					/*check if the intersection point is within the sweep of the line segment.
					For the sweeping line segment p-q, the intersection point (x) has to be on the same side of the moving line with 
					the other end point. It leads to four conditions: 
					1) the visual angle of x-p-q has to be smaller than the visual angle of p'-p-q where p' is a point on the p's projectile, 
					2) the visual angle of x-q-p has to be smaller than the visual angle of q'-q-p where q' is a point on the q's projectile,
					3) x is on the same side with p' with respect to p-q, and
					4) x is on the same side with q' with respect to p-q.*/
					CParticleF x(ol.m_X+t0*ul.m_X, ol.m_Y+t0*ul.m_Y);//the location of intersection in 2D
					CParticleF op2 = particles[j]->move(1.0f);
					CParticleF oq2 = particles[j]->next->move(1.0f);
					float va1 = GetVisualAngle2(x.m_X, x.m_Y, oq.m_X, oq.m_Y, op.m_X, op.m_Y);
					float va2 = GetVisualAngle2(op2.m_X, op2.m_Y, oq.m_X, oq.m_Y, op.m_X, op.m_Y);
					float va3 = GetVisualAngle2(x.m_X, x.m_Y, op.m_X, op.m_Y, oq.m_X, oq.m_Y);
					float va4 = GetVisualAngle2(oq2.m_X, oq2.m_Y, op.m_X, op.m_Y, oq.m_X, oq.m_Y);
					bool c1 = cos(va1) > cos(va2); //condition 1
					bool c2 = cos(va3) > cos(va4); //condition 2
					bool c3 = sin(va1)*sin(va2)>0; //condition 3
					bool c4 = sin(va3)*sin(va4)>0; //condition 4
					if(c1 && c2 && c3 && c4)
					{
						t = t0;
						res.first = t;
						res.second = particles[i];
					}
				}
			}
		}
	}
	return res;
}

int
DeformingPolygon::sanityCheck()
{
	int error_code = 0;
	map<MovingParticle*,bool> visited;
	{
		for(int i=0; i<particles.size(); ++i)
		{
			visited[particles[i]] = false;
		}
		int count = 0;
		MovingParticle* start = particles[0];
		MovingParticle* p = start;
		do
		{
			visited[p] = true;
			p = p->next;
			count++;
		}
		while(count < particles.size() && p != start);
		if(count < particles.size()) //premature ending
		{
			error_code = 1;
			return error_code;
		}
		else if(p != start) //unfinished
		{
			error_code = 2;
			return error_code;
		}
	}
	{
		for(int i=0; i<particles.size(); ++i)
		{
			visited[particles[i]] = false;
		}
		int count = 0;
		MovingParticle* start = particles[0];
		MovingParticle* p = start;
		do
		{
			visited[p] = true;
			p = p->prev;
			count++;
		}
		while(count < particles.size() && p != start);
		if(count < particles.size()) //premature ending
		{
			error_code = 3;
			return error_code;
		}
		else if(p != start) //unfinished
		{
			error_code = 4;
			return error_code;
		}
	}
	return error_code;
}

void
quickFinish(vector<MovingParticle*> particles)
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	if(particles.size()==2)
	{
		MovingParticle* p1 = factory->makeParticle(particles[0]->p, Axis);
		p1->p = particles[1]->p;
		CParticleF v = NormalizedDirection(p1->p, p1->p0);
		p1->v[0] = v.m_X;
		p1->v[1] = v.m_Y;
	}
	else if(particles.size()==3)
	{
		MovingParticle* p0 = factory->makeParticle(particles[0]->p, Axis);
		MovingParticle* p1 = factory->makeParticle(particles[1]->p, Axis);
		MovingParticle* p2 = factory->makeParticle(particles[2]->p, Axis);
		CParticleF f((p0->p.m_X+p1->p.m_X+p2->p.m_X)/3.0, (p0->p.m_Y+p1->p.m_Y+p2->p.m_Y)/3.0);
		p0->p = f;
		p1->p = f;
		p2->p = f;
	}
}

vector<MovingParticle*>
vectorize(MovingParticle* start)
{
	MovingParticle* p = start;
	vector<MovingParticle*> tr;
	set<MovingParticle*> pset;
	bool success = true;
	do
	{
		tr.push_back(p);
		if(pset.find(p) != pset.end()) //premature loop is found
		{
			success = false;
			break;
		}
		if(p->next == NULL)
		{
			success = false;
			break;
		}
		pset.insert(p);
		p = p->next;
	}
	while(p != start);

	if(success == false)
	{
		printf("failed vectorization data.\n");
		for(int i=0; i<tr.size(); ++i)
		{
			printf("%d %f %f %d\n", i+1, tr[i]->p.m_X, tr[i]->p.m_Y, tr[i]->id);
		}
		mexErrMsgTxt("vectorize: failed to vectorize a shape.");
	}
	return tr;
}

/*
remove duplicates and kinks. 
*/
void 
removeDegenerate(vector<MovingParticle*>& particles, float eps)
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	while(particles.size()>1) //at least two points are needed. else particles can vanish.
	{
		bool bchanged = false;
		for(int i=0; i<particles.size(); ++i)
		{
			MovingParticle* p = particles[i];
			MovingParticle* q = p->next;
			MovingParticle* r = p->prev;
			float dpq = Distance(p->p, q->p);
			float dpr = Distance(p->p, r->p);
			float dqr = Distance(q->p, r->p);
			if(dpq< eps) //points are close enough
			{
				//remove the two identical particles and create a new one at the same position.
				particles[i] = factory->makeParticle(p->p, Merge);
				particles[i]->next = q->next;
				particles[i]->prev = p->prev;
				particles[i]->next->prev = particles[i];
				particles[i]->prev->next = particles[i];
				int i2 = (i + 1) % particles.size();
				particles.erase(particles.begin()+i2);
				bchanged = true;
				break;
			}
			else
			{
				pair<float,float> param = _IntersectConvexPolygon::intersect(p->p0, p->p, q->p0, q->p);
				//just in case when particles cross over
				if(param.first>0 && param.first <=1.0 && param.second>0 && param.second<=1.0)
				{
					//remove the two identical particles and create a new one at the same position.
					float x = (1.0-param.first)*p->p0.m_X + param.first*p->p.m_X;
					float y = (1.0-param.first)*p->p0.m_Y + param.first*p->p.m_Y;
					particles[i] = factory->makeParticle(CParticleF(x, y), Merge);
					particles[i]->next = q->next;
					particles[i]->prev = p->prev;
					particles[i]->next->prev = particles[i];
					particles[i]->prev->next = particles[i];
					int i2 = (i + 1) % particles.size();
					particles.erase(particles.begin()+i2);
					bchanged = true;
					break;
				}
				//two neighbors are too close -> angle too small
				//else if(dpr > eps && GetVisualAngle(q->p.m_X, q->p.m_Y, r->p.m_X, r->p.m_Y, p->p.m_X, p->p.m_Y) < 0.001)
				else if(dpr > eps && (Distance2LineSegment(p->p, q->p, r->p)<eps || Distance2LineSegment(p->p, r->p, q->p)<eps))
				{
					MovingParticle* z = factory->makeParticle(p->p, Collide);
					if(dpq < dpr)//move it to the closest neighbor
					{
						z->p = q->p;
					}
					else
					{
						z->p = r->p;
					}
					//one of the neighbor (the closest one) is at the same location.
					//thus, they will be replace by a new particle on the next iteration after 'break'
					z->next = q;
					z->prev = r;
					z->next->prev = z;
					z->prev->next = z;
					particles[i] = z;
					bchanged = true;
					break;
				}
			}
		}
		if(bchanged == false) break;
	}
	//set velocity of new particles after all degenerates are removed.
	for(int i=0; i<particles.size(); ++i)
	{
		if(particles[i]->v[0]==0 && particles[i]->v[1]==0) //just being added.
		{
			particles[i]->setVelocity();
		}
	}
}

/*
Go through a chanin of points (in PARTICLES) with possible splits, separate it into a set of polygons.
Note that polygons can include a degenerate one with only two points whena a split spans multiple points (where a skeleton
is formed.)
CMAP gives pairs of points at splits. If cmap[x]==y, then cmap[y]==x. If it is not a split point, then cmap[x] is NULL.
*/
vector<vector<MovingParticle*>>
tracePolygon(vector<MovingParticle*>& particles,
			 map<MovingParticle*,MovingParticle*>& cmap)
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	vector<vector<MovingParticle*>> polygons;
	stack<vector<MovingParticle*>> partials;
	vector<MovingParticle*> trace;
	for(int i=0; i<particles.size(); ++i)
	{
		MovingParticle* p = particles[i];
		if(cmap[p] == NULL || trace.empty())
		{
			trace.push_back(p);
		}
		else
		{
			if(cmap[p] == trace[0]) //complete the polygon
			{
				//at the junction, make a new particle, set neighbors, and replace the existing one.
				MovingParticle* q = factory->makeParticle(p->p, Split);
				setNeighbors(q, p->prev, trace[0]->next);
				trace[0] = q;
				polygons.push_back(trace);
			}
			else
			{
				if(partials.empty())
				{
					partials.push(trace);
				}
				else
				{
					//check if the top of the stack can complete the loop
					vector<MovingParticle*> top = partials.top();
					if(cmap[top[0]] == p)
					{
						//make a new particle, set neighbors, and replace the existing one.
						MovingParticle* q = factory->makeParticle(p->p, Split);
						setNeighbors(q, p->prev, top[0]->next);
						top[0] = q;
						//make another new particle, set neighbors, and replace the existing one.
						MovingParticle* q2 = factory->makeParticle(trace[0]->p, Split);
						setNeighbors(q2, top[top.size()-1], trace[0]->next);
						trace[0] = q2;

						top.insert(top.end(), trace.begin(), trace.end());
						polygons.push_back(top);
						partials.pop(); //remove the top 
						if(q2->id==2150)
						{
							for(int k=0; k<top.size(); ++k)
							{
								printf("%f %f %f %f %d\n", top[k]->p.m_X, top[k]->p.m_Y, top[k]->v[0], top[k]->v[1], top[k]->id);
							}
						}
					}
					else
					{
						partials.push(trace);
					}
				}
			}
			trace.clear();
			trace.push_back(p);
		}
	}
	//the stack contains a remaining polygon.
	while(partials.empty() == false)
	{
		vector<MovingParticle*> tr = partials.top();
		MovingParticle* q = factory->makeParticle(trace[0]->p, Split);
		MovingParticle* p = tr[tr.size()-1];
		setNeighbors(q, p, trace[0]->next);
		trace[0] = q;
		trace.insert(trace.begin(), tr.begin(), tr.end());
		partials.pop();
	}
	if(trace.empty()==false)
	{
		if(trace[0]->prev != trace[trace.size()-1])
		{
			MovingParticle* q = factory->makeParticle(trace[0]->p, Split);
			MovingParticle* p = trace[trace.size()-1];
			setNeighbors(q, p, trace[0]->next);
			trace[0] = q;
		}
		polygons.push_back(trace);
	}
	return polygons;
}

/*
Loop through the current polygon, find any points that have overlaps (either splitting by a concave edge or merging by parallel edges),
and collect entry and exiting points of each overlap. 
For each entry and exiting point, provide a bridging particle to be inserted.
After the bridges are inserted, trace the particles once more and divide it into multiple polygons.
*/
vector<DeformingPolygon>
fixPolygon(DeformingPolygon& polygon, float eps, bool debugOn)
{
	if(debugOn)
	{
		printf("Before removing degenerate.\n");
		for(int i=0; i<polygon.particles.size(); ++i)
		{
			MovingParticle* p = polygon.particles[i];
			printf("%d %f %f %f %f %f %f %d\n", i+1, p->p0.m_X, p->p0.m_Y, p->p.m_X, p->p.m_Y, p->v[0], p->v[1], p->id);
		}
	}
	removeDegenerate(polygon.particles, eps);
	if(debugOn)
	{
		printf("After removing degenerate.\n");
		for(int i=0; i<polygon.particles.size(); ++i)
		{
			MovingParticle* p = polygon.particles[i];
			printf("%d %f %f %f %f %f %f %d\n", i+1, p->p0.m_X, p->p0.m_Y, p->p.m_X, p->p.m_Y, p->v[0], p->v[1], p->id);
		}
	}

	ParticleFactory* factory = ParticleFactory::getInstance();
	int N = polygon.particles.size();
	map<MovingParticle*,MovingParticle*> cmap;
	vector<MovingParticle*> dummy;

	//First check if any overlap and place dummy if necessary.
	for(int i=0; i<N; ++i)
	{
		cmap[polygon.particles[i]] = NULL;
	}
	for(int i=0; i<N; ++i)
	{
		MovingParticle* p = polygon.particles[i];
		for(int j=0; j<N; ++j)
		{
			MovingParticle* q = polygon.particles[j];
			if(p==q || q->next==p) continue;
			float sep = Distance(p->p,q->p);
			float sep2 = Distance(p->p,q->next->p);
			if(sep < eps) 
			{
				cmap[p] = q;
				cmap[q] = p;
			}
			else if(sep2 < eps)
			{
				cmap[p] = q->next;
				cmap[q->next] = p;
			}
			else if(overlap(p->p, q->p, q->next->p, p->p0, eps))//needs a dummy...
			{
				MovingParticle* x = factory->makeParticle(p->p, Dummy);
				setNeighbors(x, q, q->next);
				dummy.push_back(x);
				cmap[p] = x;
				cmap[x] = p;
			}
			if(cmap[p] != NULL) break;
		}
	}

	vector<MovingParticle*> particles = vectorize(polygon.particles[0]);
	if(debugOn)
	{
		printf("After insertion of dummies.\n");
		for(int i=0; i<particles.size(); ++i)
		{
			MovingParticle* p = particles[i];
			printf("%d %f %f %f %f %f %f %d\n", i+1, p->p0.m_X, p->p0.m_Y, p->p.m_X, p->p.m_Y, p->v[0], p->v[1], p->id);
		}
	}
	vector<vector<MovingParticle*>> polygons = tracePolygon(particles, cmap);
	if(debugOn)
	{
		printf("After tracing and splitting polygons.\n");
		for(int j=0; j<polygons.size(); ++j)
		{
			printf("Polygon %d\n", j+1);
			for(int i=0; i<polygons[j].size(); ++i)
			{
				MovingParticle* p = polygons[j][i];
				printf("%d %f %f %f %f %f %f %d\n", i+1, p->p0.m_X, p->p0.m_Y, p->p.m_X, p->p.m_Y, p->v[0], p->v[1], p->id);
			}
		}
	}

	//figure out which shape to keep as the current
	int keep=-1;
	for(int k=0; k<polygons.size(); ++k)
	{
		if(keep<0 || polygons[k].size() > polygons[keep].size())
		{
			keep = k;
		}
	}

	vector<DeformingPolygon> shapes;
	polygon.particles.clear();
	for(int i=0; i<polygons.size(); ++i)
	{
		if(polygons[i].size() == 2)
		{
			//a skeleton. 
			MovingParticle* p = factory->makeParticle(polygons[i][0]->p, Axis);
			p->p = polygons[i][1]->p;
		}
		else if(i == keep)
		{
			polygon.particles = polygons[i];
		}
		else
		{
			DeformingPolygon dp;
			dp.particles = polygons[i];
			shapes.push_back(dp);
		}
	}

	return shapes;
}


void
_trace(Vertex<CParticleF>* u, Vertex<CParticleF>*prev, vector<CParticleF>& points)
{
	points.push_back(u->key);
	u->color = Black;
	vector<Edge<CParticleF>*> edges;
	if(prev == NULL)
	{
		edges = u->aList;
	}
	else //sort edges in clock wise order
	{
		vector<pair<float,Edge<CParticleF>*>> pairs;
		for(int i=0; i<u->aList.size(); ++i)
		{
			Vertex<CParticleF>* v = u->aList[i]->v;
			float ang = GetVisualAngle2(prev->key.m_X, prev->key.m_Y, v->key.m_X, v->key.m_Y, u->key.m_X, u->key.m_Y);
			if(ang <= 0) ang += 2*PI;
			pairs.push_back(pair<float,Edge<CParticleF>*>(ang, u->aList[i]));
		}
		sort(pairs.begin(), pairs.end());
		for(int i=0; i<pairs.size(); ++i)
		{
			u->aList[i] = pairs[i].second;
		}
	}

	for(int i=0; i<u->aList.size(); ++i)
	{
		Vertex<CParticleF>* v = u->aList[i]->v;
		if(v->color == White)
		{
			_trace(v, u, points);
			points.push_back(u->key);
		}
	}
}

vector<CParticleF>
traceTree(vector<Vertex<CParticleF>*>& tree)
{
	for(int i=0; i<tree.size(); ++i)
	{
		tree[i]->Reset();
	}

	vector<CParticleF> points;
	_trace(tree[0], NULL, points);
	//remove a copy of the first one at the end of the trace
	points.erase(points.end()-1);
	return points;
}

vector<CParticleF>
offsetPolygon(vector<CParticleF>& points, float margin, float eps)
{
	vector<CParticleF> poly;
	for(int i=0; i<points.size(); ++i)
	{
		int i0 = (i-1+points.size()) % points.size();
		int i2 = (i+1) % points.size();
		CParticleF o = points[i];
		CParticleF a = points[i0];
		CParticleF b = points[i2];
		if(Distance(a, o) < eps || Distance(b, o) < eps) 
		{
			continue;
		}
		if(Distance(a, b) > eps)
		{
			float vx, vy;
			calculateVelocity(a, o, b, vx, vy);
			CParticleF q(o.m_X + margin * vx, o.m_Y + margin * vy);
			poly.push_back(q);
			//printf("o[%3.3f,%3.3f] -> [%3.3f,%3.3f]\n", o.m_X, o.m_Y, q.m_X, q.m_Y);
		}
		else //this is a leaf node of the tree.
		{
			float ang = GetVisualDirection(o.m_X, o.m_Y, a.m_X,  a.m_Y);
			float angles[] = {ang+PI/4.0, ang-PI/4.0};
			for(int j=0; j<2; ++j)
			{
				CParticleF q(o.m_X+sqrt(2.0) * margin*cos(angles[j]), o.m_Y+sqrt(2.0)*margin*sin(angles[j]));
				//printf("\to[%3.3f,%3.3f] -> [%3.3f,%3.3f]\n", o.m_X, o.m_Y, q.m_X, q.m_Y);
				poly.push_back(q);
			}
		}
	}
	return poly;
}

}
