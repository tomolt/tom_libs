struct lifetime {
	float lifetimeMin;
	float lifetimeMax;
	bool  oneshot;
	int   numParticles;
	float scaleInTime;
	float scaleOutTime;
};

struct fading {
	float fadeInTime;
	float fadeOutTime;
};

struct coloring {
	float hueVariationMin;
	float hueVariationMax;
	v4    colorProgressionMin;
	v4    colorProgressionMax;
	int   numVariants;
};

struct emission {
	emissionShape;
};

struct direction {
	v3 localX;
	v3 localY;
	v3 localZ;
	emitDir;
	emitSpread;
};

struct linear_velocity {
	linearVelocityMin;
	linearVelocityMax;
};

struct angular_velocity {
	angularVelocityMin;
	angularVelocityMax;
};

struct orbit {
	float orbitVelocityMin;
	float orbitVelocityMax;
};

struct linear_acceleration {
	v3    gravity;
	float linearDamping;
	float linearAccelMin;
	float linearAccelMax;
	float tangentAccelMin;
	float tangentAccelMax;
};

struct angular_acceleration {
	float angularDamping;
	float angularAccelMin;
	float angularAccelMax;
};

struct particles {
	float *lifetimes;
	v3    *positions;
	v3    *linearVelocities;
	v3    *angularVelocities;
	v4    *colors;
	int   *variants;
};

void apply_linear_acceleration()
{
	linearVelocity *= linearDamping;
	linearVelocity += gravity;
	linearVelocity *= 1.0 + linearAccel / length(linearVelocity);
}

void apply_angular_acceleration()
{
}

void apply_linar_velocity()
{
	position += linearVelocity;
}
