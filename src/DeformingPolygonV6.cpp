#include <DeformingPolygonV5.h>
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
	int poid = 0; //797; //73; //836; //122; //135;
	int paid = 98; 
	int paid2 = 71; //1295;

	EventStruct::EventStruct(float t, EventType type, MovingParticle* p, MovingParticle* q)
	{
		this->t = t;
		this->type = type;
		this->p = p;
		this->q = q;
		if(q == NULL)
		{
			r = NULL;
		}
		else
		{
			r = q->next;
		}
	}

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

CParticleF
crossProduct(CParticleF& a, CParticleF& b)
{
	return CParticleF(a.m_Y*b.m_Z-a.m_Z*b.m_Y, a.m_Z*b.m_X-a.m_X*b.m_Z, a.m_X*b.m_Y-a.m_Y*b.m_X);
}


float
dotProduct(CParticleF& a, CParticleF& b)
{
	return a.m_X*b.m_X + a.m_Y*b.m_Y + a.m_Z*b.m_Z;
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

CParticleF
centroidPoint(vector<CParticleF>& poly)
{
	CParticleF c(0, 0, 0);
	for(int i=0; i<poly.size(); ++i)
	{
		c.m_X += poly[i].m_X;
		c.m_Y += poly[i].m_Y;
		c.m_Z += poly[i].m_Z;
	}
	c.m_X /= (float)poly.size();
	c.m_Y /= (float)poly.size();
	c.m_Z /= (float)poly.size();
	return c;
}

/*
Check if a-b-c lie on a line.
*/
bool
coLinear(CParticleF& a, CParticleF& b, CParticleF& c, float precision = 1.0e-3)
{
	float d = Distance2Line(a, c, b);
	return d < precision;
}

/*
Does this return true when the point is on the polygon?
*/
bool
inside2(const CParticleF& p, CParticleF pnts[4])
{
	bool c = false;
	float x = p.m_X;
	float y = p.m_Y;
	int size = 4;
	for (int i = 0, j = size-1; i < size; j = i++) 
	{
		float xi = pnts[i].m_X;
		float yi = pnts[i].m_Y;
		float xj = pnts[j].m_X;
		float yj = pnts[j].m_Y;
		if ( ((yi>y) != (yj>y)) &&
			(x <= (float)(xj-xi) * (float)(y-yi) / (float)(yj-yi) + xi) )
			c = !c;
	}
	return c;
}

/*
Find the time to hit for the 1st particle (P) to a slanted plane formed with Q and R.
Returns Inf if no intersection within the bounded polygon.
*/
float
intersectPolygonAndLine(MovingParticle* p, MovingParticle* q, MovingParticle* r, float time = 20.0f)
{
	CParticleF po = p->p;
	CParticleF qo = q->p;
	CParticleF ro = r->p;
	//perp to plane
	float eps = 1.0e-5;
	float t = std::numeric_limits<float>::infinity();
	//if(coLinear(p->p, q->p, r->p, eps)==false) //if colinear, no chance of intersection
	{
		float d = time; //Max(Distance(po, qo), Distance(po, ro)); //find the bounding limit
		CParticleF poly[4];
		//make a 3D plane with a unit-slant
		poly[0] = qo; poly[1] = ro; 
		poly[2].m_X=ro.m_X + d * r->v[0];
		poly[2].m_Y=ro.m_Y + d * r->v[1];
		poly[2].m_Z=d;
		poly[3].m_X=qo.m_X + d * q->v[0];
		poly[3].m_Y=qo.m_Y + d * q->v[1];
		poly[3].m_Z=d;

		CParticleF u(p->v[0], p->v[1], 1.0f);
		CParticleF m;
		for(int i=0; i<4; ++i)
		{
			m.m_X += poly[i].m_X;
			m.m_Y += poly[i].m_Y;
			m.m_Z += poly[i].m_Z;
		}
		m.m_X /= 4.0f;
		m.m_Y /= 4.0f;
		m.m_Z /= 4.0f;

		CParticleF z = perp2Plane(poly[0], poly[2], poly[3]);
		float t0 = intersectPlaneAndLine(m, z, po, u);
		CParticleF x(po.m_X + t0*u.m_X, po.m_Y + t0*u.m_Y);
		for(int i=0; i<4; ++i)
		{
			poly[i].m_Z = 0;
		}
		if(inside2(x, poly))
		{
			t=t0;
		}
	}
	return t;
}

bool
calculateVelocity(CParticleF& a, CParticleF& o, CParticleF& b, float& vx, float& vy)
{
	CParticleF bs = bisector(o, a, b);
	float ang = GetVisualAngle2(a.m_X, a.m_Y, b.m_X, b.m_Y, o.m_X, o.m_Y);
	float cs = cos((PI - Abs(ang)) / 2.0);
	bool bret = true;
	if(cs < 0.01)
	{
		cs = 0.01;
		bret = false;
	}
	float len = 1.0f / cs;
	bs.m_X *= len;
	bs.m_Y *= len;
	vx = bs.m_X;
	vy = bs.m_Y;
	return bret;
}

bool 
MovingParticle::setVelocity()
{
	if(next && prev)
	{
		this->bStable = calculateVelocity(prev->p, p, next->p, v[0], v[1]);
	}
	else
	{
		mexErrMsgTxt("setVelocity: an isolated particle found.");		
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
	next->bDirty = true;
	prev->bDirty = true;
}

void
setRelation(MovingParticle* child, MovingParticle* parent)
{
	if(child->type == Dummy)
	{
		if(Distance(child->p, child->prev->p) < Distance(child->p, child->next->p))
		{
			child = child->prev;
		}
		else
		{
			child = child->next;
		}
	}
	parent->descendents.push_back(child);
	child->anscestors.push_back(parent);
}

bool clockWise(vector<MovingParticle*>& particles)
{
	vector<CParticleF> pnts;
	for(int i=0; i<particles.size(); ++i)
	{
		pnts.push_back(particles[i]->p);
	}
	return ClockWise(pnts);
}

DeformingPolygon::DeformingPolygon(vector<CParticleF>& pnts, float tm, DeformingPolygon* pa)
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	for(int i=0; i<pnts.size(); ++i)
	{
		MovingParticle* p = factory->makeParticle(pnts[i], Regular, tm);
	}
	_setMap(); //set particle-index map
	for(int i=0; i<particles.size(); ++i)
	{
		//particles[i]->p0 = particles[i]->p;
		int j = (i+1) % particles.size();
		particles[i]->next = particles[j];
		int k = (i-1+particles.size()) % particles.size();
		particles[i]->prev = particles[k];
		particles[i]->setVelocity();
	}
	created = tm;
	time = tm;
	orientation = ClockWise(pnts);
	id = _id++;
	parent = pa;
	died = 0;
}


DeformingPolygon::DeformingPolygon(vector<MovingParticle*>& pnts, float tm, DeformingPolygon* pa)
{
	this->particles = pnts;
	_setMap(); //set particle-index map
	for(int i=0; i<particles.size(); ++i)
	{
		//particles[i]->p0 = particles[i]->p;
		int j = (i+1) % particles.size();
		particles[i]->next = particles[j];
		int k = (i-1+particles.size()) % particles.size();
		particles[i]->prev = particles[k];
		particles[i]->setVelocity();
	}
	created = tm;
	time = tm;
	orientation = clockWise(particles);
	id = _id++;
	parent = pa;
	died = 0;
}

EventStruct
NextEvent(MovingParticle* p, vector<MovingParticle*>& particles, float period)
{
	float ptime = p->time;
	float eps = 1.0e-3;
	EventStruct ev(std::numeric_limits<float>::infinity(), UnknownEvent, p);
	{
		CParticleF po = p->p;
		//CParticleF u(p->v[0], p->v[1], 1.0);
		for(int j=0; j<particles.size(); ++j)
		{
			MovingParticle* q = particles[j];
			MovingParticle* r = q->next;
			if(p->id==paid && q->id==paid2)
			{
				j+=0;
			}
			if(p==q || p->prev==q) continue;
			CParticleF p2(po.m_X+p->v[0], po.m_Y+p->v[1]);
			CParticleF qo = q->p;
			CParticleF q2(qo.m_X+q->v[0], qo.m_Y+q->v[1]);
			CParticleF ro = r->p;
			
			float dist = Distance2LineSegment(qo, ro, po);
			float t1=std::numeric_limits<float>::infinity(), t2=std::numeric_limits<float>::infinity(), t3=std::numeric_limits<float>::infinity();
			pair<float,float> param;
			{
				float t1a = intersectPolygonAndLine(p, q, r, 2000.0f); // period - p->time);
				if(t1a >= 0)
				{
					t1 = t1a;
				}
			}
			{
				param = _IntersectConvexPolygon::intersect(po, p2, qo, q2);
				//The second condition is needed to handle inaccurate velocity resulted from removeUnstable function,
				//which remove a particle with tiny angle to prevent numerically unstable velocity.
				///As a result, a particle that replaces the unstable one may not satisfy the first condition.
				if(Abs(param.first - param.second) < 1.0e-5 || q==p->next)
				{
					if(param.first>=0)
					{
						t2 = param.first; 
					}
				}
			}
			if(dist < eps) //take care of a case where the particle is on another line segment.
			{
				t3 = dist;
			}
			float t0 = Min(Min(t1, t2), t3);
			if(p->id==paid && q->id==paid2)
			{
				pair<float,float> param = _IntersectConvexPolygon::intersect(po, p2, qo, q2);
				float tt = intersectPolygonAndLine(p, q, r, 2000.0f);
				j+=0;
				/*printf("%f %f\n", param.first, param.second);
				p->print();
				q->print();
				mexErrMsgTxt("hello");*/
			}
			if(t0 >= 0) 
			{
				float t = t0 + ptime;
				if(t < ev.t)
				{
					if(q == p->next)
					{
						ev = EventStruct(t, EdgeEvent, p, particles[j]);
					}
					else
					{
						ev = EventStruct(t, SplitEvent, p, particles[j]);
					}
				}
				else if (t == ev.t && t < std::numeric_limits<float>::infinity()) //collision has a higher precedence than split
				{
					if (p == NULL)
					{
						p = NULL;
					}
					else if (q == p->next)
					{
						ev = EventStruct(t, EdgeEvent, p, particles[j]);
					}
				}
			}
		}
	}
	//ev.t = Max(ptime, ev.t);
	p->bDirty = false;
	return ev;
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
DeformingPolygon::quickFinish()
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	if(particles.size()==1)
	{
		if(particles[0]->type != Axis)
		{
			MovingParticle* p = factory->makeParticle(particles[0]->p, Axis, time);
			setRelation(particles[0], p);
			particles[0]->bActive = false;
			p->bActive = false;
		}
	}
	if(particles.size()==2)
	{
		MovingParticleType type = particles[0]->type==Merge ? Merge: Axis;
		float x1 = (particles[0]->p.m_X + particles[1]->p.m_X) / 2;
		float y1 = (particles[0]->p.m_Y + particles[1]->p.m_Y) / 2;
		CParticleF q(x1, y1);
		MovingParticle* pa = factory->makeParticle(q, type, time);
		for(int i=0; i<2; ++i)
		{
			particles[i]->p = q;
			setRelation(particles[i], pa);
			particles[i]->bActive = false;
		}
		pa->bActive = false;
	}
	else if(particles.size()==3)
	{
		float t = std::numeric_limits<float>::quiet_NaN();
		MovingParticle* p = NULL;
		MovingParticle* q = NULL;
		MovingParticle* r = NULL;

		for(int n=0; n<3; ++n)
		{
			p = particles[n];
			q = particles[(n+1) % 3];
			r = particles[(n+2) % 3];
			pair<float,float> param = _IntersectConvexPolygon::intersect(p->p0, p->move(1.0), q->p0, q->move(1.0));
			if(Abs(param.first) < Distance(p->p, r->p) )
			{
				t = param.first; 
				break;
			}
		}
		MovingParticle* pnew = NULL;
		if(t==t){
			float x1 = (1.0-t) * p->p0.m_X + t * p->move(1.0).m_X;
			float y1 = (1.0-t) * p->p0.m_Y + t * p->move(1.0).m_Y;
			CParticleF f(x1,y1);
			pnew = factory->makeParticle(f, Axis, time);
		}
		else
		{
			CParticleF f((particles[0]->p.m_X+particles[1]->p.m_X+particles[2]->p.m_X)/3.0, (particles[0]->p.m_Y+particles[1]->p.m_Y+particles[2]->p.m_Y)/3.0);
			pnew = factory->makeParticle(f, Axis, time);
		}
		for(int i=0; i<3; ++i)
		{
			//particles[i]->p=f; ///TK
			setRelation(particles[i], pnew);
			particles[i]->bActive = false;
		}
		pnew->bActive = false;
	}
	died = time;
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

MovingParticle*
DeformingPolygon::applyInterval(MovingParticle* p, float delta)
{
	assert(pmap.find(p) != pmap.end() && pmap.find(p->next) != pmap.end());
	for (int i = 0; i<particles.size(); ++i)
	{
		particles[i]->update(delta);
	}
	time += delta;
	return p;
}

MovingParticle*
DeformingPolygon::applyCollision(MovingParticle* p, float delta)
{
	assert(pmap.find(p) != pmap.end() && pmap.find(p->next) != pmap.end());
	for(int i=0; i<particles.size(); ++i)
	{
		particles[i]->update(delta);
	}
	ParticleFactory* factory = ParticleFactory::getInstance();
	MovingParticle* pnew = factory->makeParticle(p->p, Collide, time + delta);
	setNeighbors(pnew, p->prev, p->next->next);
	pnew->setVelocity();
	setRelation(p, pnew);
	setRelation(p->next, pnew);
	time += delta;
	p->bActive = false;
	p->next->bActive = false;
	return pnew;
}

pair<MovingParticle*,MovingParticle*>
DeformingPolygon::applySplit(MovingParticle* p, MovingParticle* q, float delta)
{
	assert(pmap.find(p) != pmap.end() && pmap.find(q) != pmap.end());
	for(int i=0; i<particles.size(); ++i)
	{
		particles[i]->update(delta);
	}
	MovingParticle* r = q->next;
	ParticleFactory* factory = ParticleFactory::getInstance();
	MovingParticle* pnew[2];
	pnew[0] = factory->makeParticle(p->p, Split, time + delta);
	pnew[1] = factory->makeParticle(p->p, Split, time + delta);
	setNeighbors(pnew[0], p->prev, r);
	setNeighbors(pnew[1], q, p->next);
	for(int i=0; i<2; ++i)
	{
		pnew[i]->setVelocity();
		/*if (Distance(p->p, q->p) < Distance(p->p, r->p))
		{
			setRelation(q, pnew[i]);
		}
		else
		{
			setRelation(r, pnew[i]);
		}*/
		setRelation(p, pnew[i]);
		//make sure the descendents are in the right order
		/*MovingParticle* pd = pnew[i]->prev->descendents[pnew[i]->prev->descendents.size() - 1];
		if (Distance(pd->p0, pnew[i]->descendents[1]->p0) < Distance(pd->p0, pnew[i]->descendents[0]->p0))
		{
			swap(pnew[i]->descendents[0], pnew[i]->descendents[1]);
		}*/
	}

	time += delta;
	p->bActive = false;
	return pair<MovingParticle*,MovingParticle*>(pnew[0], pnew[1]);
}

bool need2Update(map<MovingParticle*,int>& pmap, MovingParticle* p)
{
	if(p->bDirty) return true;
	EventStruct ev = p->event;
	if(ev.type == EdgeEvent)
	{
		if(pmap.find(ev.p)!=pmap.end() && pmap.find(ev.q)!=pmap.end())
		{
			return false;
		}
		else return true;
	}
	else if(ev.type == SplitEvent)
	{
		if(pmap.find(ev.p)!=pmap.end() && pmap.find(ev.q)!=pmap.end() && pmap.find(ev.r)!=pmap.end())
		{
			return false;
		}
		else return true;
	}
	else
	{
		return true;
	}
}

/*
remove duplicates and kinks. 
*/
void
DeformingPolygon::removeUnstables()
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	float v[2];
	while(particles.size()>=3)
	{
		bool bChanged = false;
		for(int i=0; i<particles.size(); ++i)
		{
			MovingParticle* p0 = particles[i];
			if (p0->bStable == false || 
				calculateVelocity(p0->prev->p, p0->p, p0->next->p, v[0], v[1]) == false)
				//coLinear(p0->prev->p, p0->next->p, p0->p))
			{
				p0->bActive = false;
				MovingParticle* prev = p0->prev;
				MovingParticle* next = p0->next;
				float dn = Distance(p0->p, next->p);
				float dp = Distance(p0->p, prev->p);
				if (dn <= dp)
				{
					MovingParticle* q = factory->makeParticle(next->p, Merge, time); // p0->next->type, time);
					setNeighbors(q, prev, next->next);
					setRelation(next, q);
					q->bDirty = true;
					p0->next->bActive = false;
					next = q; //IMPORTANT ?
					particles = vectorize(q);
				}
				if (dp <= dn)
				{
					MovingParticle* p = factory->makeParticle(prev->p, Merge, time); // p0->prev->type, time);
					setNeighbors(p, prev->prev, next);
					setRelation(prev, p);
					p->bDirty = true;
					p->setVelocity();
					prev->bActive = false;
					particles = vectorize(p);
				}
				bChanged = true;

				break;
			}
		}
		if(bChanged == false) break;
	}

	_setMap();
	for (int i = 0; i < particles.size(); ++i)
	{
		if (particles[i]->bDirty)
		{
			particles[i]->setVelocity();
		}
	}
}

EventStruct
findNextEvent(vector<MovingParticle*>& particles)
{
	EventStruct ev(std::numeric_limits<float>::infinity(), UnknownEvent, NULL);
	for(int i=0; i<particles.size(); ++i)
	{
		if(particles[i]->event.t < ev.t)
		{
			ev = particles[i]->event;
		}
		else if (Abs(particles[i]->event.t - ev.t) < 1.0e-5) //let collision takes precedence over split
		{
			if (ev.type == SplitEvent && particles[i]->event.type == EdgeEvent)
			{
				ev = particles[i]->event;
			}
		}
	}
	return ev;
}

/*vector<DeformingPolygon*>
DeformingPolygon::deform(float period)
{
	//printf("polygon: %d\n", this->id);
	if (id == poid)
	{
		id += 0;
	}
	vector<DeformingPolygon*> polygons(1, this);
	takeSnapshot();
	if (particles.size() <= 3)
	{
		quickFinish();
		return polygons;
	}
	for (int i = 0; i<particles.size(); ++i)
	{
		if (particles[i]->id == paid)
		{
			i += 0;
		}
		if (need2Update(pmap, particles[i]))
		{
			if (particles[i]->id == paid)
			{
				i += 0;
			}
			particles[i]->event = NextEvent(particles[i], particles, period);
		}
		if (particles[i]->id == paid)
		{
			MovingParticle* p = particles[i];
			EventStruct ev = p->event;
			EventStruct ev2 = NextEvent(particles[i], particles, period);
			i += 0;
			if (id == poid)
				i += 0;
			EventStruct ev3 = NextEvent(particles[i], particles, period);
		}
	}
	for (int i = 0; i<particles.size(); ++i)
	{
		//particles[i]->print();
		if (particles[i]->id == paid)
		{
			//printf("\traced p event: t=%f, type=%d\n", particles[i]->event.t, particles[i]->event.type);
		}
	}
	EventStruct current = findNextEvent(particles);

	float delta = current.t - time;
	if (current.t < period)
	{
		if (current.p->id == paid)
			paid += 0;
		MovingParticle* pnew[2] = { NULL, NULL };
		if (current.type == EdgeEvent)
		{
			pnew[0] = applyCollision(current.p, delta);
		}
		else if (current.type == SplitEvent)
		{
			pair<MovingParticle*, MovingParticle*> newpair = applySplit(current.p, current.q, delta);
			pnew[0] = newpair.first;
			pnew[1] = newpair.second;
			assert(pnew[0] && pnew[1]);
		}
		for (int k = 0; k<2; ++k)
		{
			if (pnew[k] != NULL)
			{
				vector<MovingParticle*> ps = vectorize(pnew[k]);
				DeformingPolygon* newpoly = new DeformingPolygon(time, clockWise(ps), this);
				newpoly->event = current;
				newpoly->particles = ps;
				newpoly->_setMap();
				children.push_back(newpoly);
				newpoly->removeUnstables();
				vector<DeformingPolygon*> newpolygons = newpoly->deform(period);
				polygons.insert(polygons.end(), newpolygons.begin(), newpolygons.end());
			}
		}
	}
	for (int i = 0; i<polygons.size(); ++i)
	{
		time = Max(time, polygons[i]->time);
	}
	died = time;
	return polygons;
}
*/

vector<DeformingPolygon*>
DeformingPolygon::deform(float period, float interval)
{
	//MovingParticle* tracedP = ParticleFactory::getInstance()->particles.size() < 63 ? NULL : ParticleFactory::getInstance()->particles[62];
	float ft = floor(time / interval);
	float nextInterval = ft * interval + interval;
	vector<DeformingPolygon*> polygons(1, this);
	takeSnapshot();
	if (particles.size() <= 2)
	{
		quickFinish();
		return polygons;
	}
	for (int i = 0; i<particles.size(); ++i)
	{
		if (need2Update(pmap, particles[i]))
		{
			particles[i]->event = NextEvent(particles[i], particles, period);
		}
	}
	//return  polygons; ///TK!!!

	EventStruct current = findNextEvent(particles);
	/*printf("%f %d %3.3f %3.3f %3.3f %3.3f\n", current.t, current.type, current.p->p0.m_X, current.p->p0.m_Y,
		current.q == NULL ? 0 : current.q->p0.m_X, current.q == NULL ? 0 : current.q->p0.m_Y);*/
	if (nextInterval < current.t)
	{
		current.type = IntervalEvent;
		current.t = nextInterval;
	}

	float delta = current.t - time;
	if (current.t < period)
	{
		MovingParticle* pnew[2] = { NULL, NULL };
		if (current.type == IntervalEvent)
		{			
			pnew[0] = applyInterval(particles[0], delta);
		}
		else if (current.type == EdgeEvent)
		{
			pnew[0] = applyCollision(current.p, delta);
		}
		else if (current.type == SplitEvent)
		{
			pair<MovingParticle*, MovingParticle*> newpair = applySplit(current.p, current.q, delta);
			pnew[0] = newpair.first;
			pnew[1] = newpair.second;
			assert(pnew[0] && pnew[1]);
		}
		for (int k = 0; k<2; ++k)
		{
			if (pnew[k] != NULL)
			{
				vector<MovingParticle*> ps = vectorize(pnew[k]);
				DeformingPolygon* newpoly = new DeformingPolygon(time, clockWise(ps), this);
				newpoly->event = current;
				newpoly->particles = ps;
				newpoly->_setMap();
				children.push_back(newpoly);
				newpoly->removeUnstables();
				if (this->id == poid)
				{
					poid += 0;
				}
				vector<DeformingPolygon*> newpolygons = newpoly->deform(period, interval);
				polygons.insert(polygons.end(), newpolygons.begin(), newpolygons.end());
			}
		}
	}
	for (int i = 0; i<polygons.size(); ++i)
	{
		time = Max(time, polygons[i]->time);
	}
	died = time;
	return polygons;
}

void
_trace(Vertex<CParticleF>* u, Vertex<CParticleF>*prev, vector<MovingParticle*>& points)
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	MovingParticle* particle = factory->makeParticle(u->key, Initial, 0.0f);
	points.push_back(particle);
	u->color = Black;
	CParticleF pu = u->key;
	CParticleF qu = prev==NULL ? CParticleF(pu.m_X-1.0, pu.m_Y): prev->key;
	while(true)
	{
		Vertex<CParticleF>* v = NULL;
		float minAngle = 2*PI;
		for(int i=0; i<u->aList.size(); ++i)
		{
			Vertex<CParticleF>* w = u->aList[i]->v;
			if(w->color == White)
			{
				float ang = GetVisualAngle2(qu.m_X, qu.m_Y, w->key.m_X, w->key.m_Y, pu.m_X, pu.m_Y);
				if(ang <= 0) ang += 2*PI;
				if(ang < minAngle) 
				{
					minAngle = ang;
					v = w;
				}
			}
		}
		if(v != NULL)
		{
			_trace(v, u, points);
			points.push_back(particle);
		}
		else
		{
			break;
		}
		qu = v->key;
	}
}

vector<MovingParticle*>
traceTree(vector<Vertex<CParticleF>*>& tree)
{
	for(int i=0; i<tree.size(); ++i)
	{
		tree[i]->Reset();
	}

	vector<MovingParticle*> points;
	_trace(tree[0], NULL, points);
	//remove a copy of the first one at the end of the trace
	points.erase(points.end()-1);
	for(int i=0; i<points.size(); ++i)
	{
		int i0 = (i-1+points.size()) % points.size();
		int i2 = (i+1) % points.size();
		setNeighbors(points[i], points[i0], points[i2]);
	}
	return points;
}

vector<MovingParticle*>
offsetPolygon(vector<MovingParticle*>& points, float margin, float eps)
{
	ParticleFactory* factory = ParticleFactory::getInstance();
	vector<MovingParticle*> poly;
	for(int i=0; i<points.size(); ++i)
	{
		int i0 = (i-1+points.size()) % points.size();
		int i2 = (i+1) % points.size();
		CParticleF o = points[i]->p;
		CParticleF a = points[i0]->p;
		CParticleF b = points[i2]->p;
		if(Distance(a, o) < eps || Distance(b, o) < eps) 
		{
			continue;
		}
		if(Distance(a, b) > eps)
		{
			float vx, vy;
			calculateVelocity(a, o, b, vx, vy);
			//CParticleF q(o.m_X + margin * vx, o.m_Y + margin * vy);
			MovingParticle* particle = factory->makeParticle(o, Regular, 0.0f); //points[i];
			//particle->p0 = o;
			particle->v[0] = vx; 
			particle->v[1] = vy;
			setRelation(points[i], particle);
			poly.push_back(particle);
		}
		else //this is a leaf node of the tree.
		{
			float ang = GetVisualDirection(o.m_X, o.m_Y, a.m_X,  a.m_Y);
			float angles[] = {ang+PI/4.0, ang-PI/4.0};
			MovingParticle* q[2];
			q[0] = points[i];
			q[1] = factory->makeParticle(o, Regular, 0.0f);
			for(int j=0; j<2; ++j)
			{
				//float vx = sqrt(2.0) * margin*cos(angles[j]);
				//float vy = sqrt(2.0) * margin*sin(angles[j]);
				float vx = sqrt(2.0) * cos(angles[j]);
				float vy = sqrt(2.0) * sin(angles[j]);
				//CParticleF q(o.m_X+vx, o.m_Y+vy);
				MovingParticle* particle = factory->makeParticle(o, Regular, 0.0f);
				//particle->p0 = o;
				particle->v[0] = vx;
				particle->v[1] = vy;
				setRelation(points[i], particle);
				poly.push_back(particle);
			}
		}
	}
	for(int i=0; i<poly.size(); ++i)
	{
		//setting the neighbors, except velocity
		int j = (i+1) % poly.size();
		int k = (i-1+poly.size()) % poly.size();
		MovingParticle* p = poly[i];
		MovingParticle* prev = poly[k];
		MovingParticle* next = poly[j];
		setNeighbors(p, prev, next);
	}
	for(int i=0; i<poly.size(); ++i)
	{
		poly[i]->update(margin);
	}
	return poly;
}

mxArray*
StoreParticles(const vector<vector<MovingParticle*>>& polygons)
{
	const int dims[] = {polygons.size()};
	mxArray* cell = mxCreateCellArray(1, (mwSize*) dims);
	for(int i=0; i<polygons.size(); ++i)
	{
		int n = polygons[i].size() ;
		const int dimsC[] = {n, 6};
		mxArray* ar = mxCreateNumericArray(2, (mwSize*) dimsC, mxSINGLE_CLASS, mxREAL);
		float* p = (float*) mxGetData(ar);
		for(int j=0; j<n; ++j)
		{
			p[j] = polygons[i][j]->p0.m_X;
			p[n+j] = polygons[i][j]->p0.m_Y;
			p[2*n+j] = polygons[i][j]->p.m_X;
			p[3*n+j] = polygons[i][j]->p.m_Y;
			p[4*n+j] = polygons[i][j]->id;
			p[5*n+j] = polygons[i][j]->type;
		}
		mxSetCell(cell, i, ar);
	}
	return cell;
}


void
traceBack(MovingParticle* p, vector<MovingParticle*>& trace, set<MovingParticle*>& pset)
{
	if (pset.find(p) == pset.end())
	{
		pset.insert(p);
		if (p->type == Regular || p->type == Initial)
		{
			trace.push_back(p);
		}
		else
		{
			for (int i = 0; i < p->descendents.size(); ++i)
			{
				traceBack(p->descendents[i], trace, pset);
			}
		}
	}
}

/*
Given a polygon, it returns a series of initial particles by tracing particles backward in time.
*/
vector<MovingParticle*>
traceBackPolygon(DeformingPolygon* polygon)
{
	vector<MovingParticle*> trace;
	set<MovingParticle*> pset;
	for (int i = 0; i<polygon->particles.size(); ++i)
	{
		MovingParticle* p = polygon->particles[i];
		traceBack(p, trace, pset);
	}
	return trace;
}


}