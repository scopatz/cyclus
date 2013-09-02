// material.cc
#include "material.h"

#include "cyc_arithmetic.h"
#include "error.h"
#include "cyc_limits.h"
#include "timer.h"
#include "logger.h"

#include <cmath>
#include <vector>
#include <list>

namespace cyclus {

std::list<Material*> Material::materials_;

bool Material::decay_wanted_ = false;

int Material::decay_interval_ = 1;

bool Material::type_is_recorded_ = false;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Material::Material() {
  SetQuantity(0);
  last_update_time_ = TI->time();
  CLOG(LEV_INFO4) << "Material ID=" << ID_ << " was created.";
  materials_.push_back(this);
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Material::~Material() {
  materials_.remove(this);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Material::Material(CompMapPtr comp) {
  SetQuantity(0);
  IsoVector vec = IsoVector(comp);
  last_update_time_ = TI->time();
  iso_vector_ = vec;
  CLOG(LEV_INFO4) << "Material ID=" << ID_ << " was created.";
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Material::Material(IsoVector vec) {
  last_update_time_ = TI->time();
  iso_vector_ = vec;
  CLOG(LEV_INFO4) << "Material ID=" << ID_ << " was created.";
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Material::Material(const Material& other) {
  iso_vector_ = other.iso_vector_;
  last_update_time_ = other.last_update_time_;
  quantity_ = other.quantity_;
  CLOG(LEV_INFO4) << "Material ID=" << ID_ << " was created.";
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Material::Absorb(Material::Ptr matToAdd) {
  // @gidden figure out how to handle this with the database - mjg
  // Get the given Material's composition.
  double amt = matToAdd->quantity();
  double ratio = ((quantity_ < cyclus::eps_rsrc()) ? 1 : amt / quantity_);
  iso_vector_.Mix(matToAdd->isoVector(),
                  ratio); // @MJG_FLAG this looks like it copies isoVector()... should this return a pointer?
  quantity_ += amt;
  CLOG(LEV_DEBUG2) << "Material ID=" << ID_ << " absorbed material ID="
                   << matToAdd->ID() << ".";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Material::Ptr Material::Extract(double mass) {
  if (quantity_ < mass) {
    std::string err = "The mass ";
    err += mass;
    err += " cannot be extracted from Material with ID ";
    err += ID_;
    throw ValueError(err);
  }
  // remove our mass
  quantity_ -= mass;
  // make a new material, set its mass
  Material::Ptr new_mat = Material::Ptr(new Material(iso_vector_));
  new_mat->SetQuantity(mass);
  // we just split a resource, so keep track of the original for book keeping
  new_mat->SetOriginalID(this->OriginalID());

  CLOG(LEV_DEBUG2) << "Material ID=" << ID_ << " had " << mass
                   << " kg extracted from it. New mass=" << quantity() << " kg.";

  return new_mat;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::map<Iso, double> Material::diff(Material::Ptr other) {
  CompMapPtr comp = CompMapPtr(other->isoVector().comp());
  double amt = other->mass(KG);
  return diff(comp, amt, KG);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::map<Iso, double> Material::diff(const CompMapPtr other, double other_amt,
                                     MassUnit
                                     unit) {

  std::map<Iso, double>::iterator entry;
  CompMapPtr orig = CompMapPtr(this->UnnormalizeComp(MASS, unit));
  std::map<Iso, double> to_ret = orig->map();
  double amt = 0;
  double orig_amt = this->mass(unit);
  for (entry = (*other).begin(); entry != (*other).end(); ++entry) {
    int iso = (*entry).first;
    if (orig->count(iso) != 0) {
      amt = (*orig)[iso] - (*entry).second * other_amt ;
    } else {
      amt = -(*entry).second * other_amt;
    }
    to_ret[iso] = amt;
  }

  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::map<Iso, double> Material::ApplyThreshold(std::map<Iso, double> vec,
                                               double threshold) {
  if (threshold < 0) {
    std::stringstream ss;
    ss << "The threshold cannot be negative. The value provided was "
       << threshold
       << " .";
    throw ValueError(ss.str());
  }
  std::map<Iso, double>::const_iterator it;
  std::map<Iso, double> to_ret;

  for (it = vec.begin(); it != vec.end(); ++it) {
    if (abs((*it).second) > threshold) {
      to_ret[(*it).first] = (*it).second;
    }
  }
  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Material::Ptr Material::Extract(const CompMapPtr remove_comp, double remove_amt,
                                MassUnit unit, double threshold) {
  CompMapPtr final_comp = CompMapPtr(new CompMap(MASS));
  std::vector<double> final_amt_vec = std::vector<double>();
  double final_amt = 0;

  std::map<Iso, double> remainder;
  remainder = ApplyThreshold(diff(remove_comp, remove_amt, unit), threshold);
  std::map<Iso, double>::iterator it;

  for (it = remainder.begin(); it != remainder.end(); ++it) {
    int iso = (*it).first;
    double amt = (*it).second;
    if (amt < 0) {
      std::stringstream ss;
      ss << "The Material " << this->ID()
         << " has insufficient material to extract "
         << " the isotope : " << iso
         << ". It has only "
         << this->mass(iso)
         << " of that iso. The difference between the amounts is : "
         << amt
         << " . ";
      throw ValueError(ss.str());
    } else {
      (*final_comp)[iso] = amt;
      final_amt_vec.push_back(amt);
    }
  }
  if (!final_amt_vec.empty()) {
    final_amt = CycArithmetic::KahanSum(final_amt_vec);
  }

  // make new material
  Material::Ptr new_mat = Material::Ptr(new Material(remove_comp));
  new_mat->SetQuantity(remove_amt, unit);
  new_mat->SetOriginalID(this->OriginalID());   // book keeping
  // adjust old material
  final_comp->normalize();
  this->iso_vector_ = IsoVector(CompMapPtr(final_comp));
  this->SetQuantity(final_amt, unit);

  CLOG(LEV_DEBUG2) << "Material ID=" << ID_ << " had composition extracted.";
  return new_mat;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Material::Print() {
  CLOG(LEV_INFO4) << "Material ID=" << ID_
                  << ", quantity=" << quantity() << ", units=" << units();

  CLOG(LEV_INFO5) << "Composition {";
  CLOG(LEV_INFO5) << Detail();
  CLOG(LEV_INFO5) << "}";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string Material::Detail() {
  std::vector<std::string>::iterator entry;
  std::vector<std::string> entries = iso_vector_.comp()->CompStrings();
  for (entry = entries.begin(); entry != entries.end(); entry++) {
    CLOG(LEV_INFO5) << "   " << *entry;
  }
  return "";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Material::operator==(const Material::Ptr other) {
  return AlmostEqual(other, 0);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Material::AlmostEqual(const Material::Ptr other, double threshold) const {
  // I learned at
  // http://www.ualberta.ca/~kbeach/comp_phys/fp_err.html#testing-for-equality
  // that the following is less naive than the naive way to do it...
  return iso_vector_.comp()->AlmostEqual(*(other->isoVector().comp()), threshold);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Material::SetQuantity(double quantity) {
  quantity_ = quantity;
  CLOG(LEV_DEBUG2) << "Material ID=" << ID_ << " had mass set to"
                   << quantity << " kg";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Material::SetQuantity(double quantity, MassUnit unit) {
  SetQuantity(ConvertToKg(quantity, unit));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::quantity() {
  return quantity_;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::mass(MassUnit unit) {
  return ConvertFromKg(quantity(), unit);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::mass(Iso tope, MassUnit unit) {
  return ConvertFromKg(mass(tope), unit);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::mass(Iso tope) {
  double to_ret;
  CompMapPtr the_comp = isoVector().comp();
  the_comp->Massify();
  if (the_comp->count(tope) != 0) {
    to_ret = the_comp->MassFraction(tope) * mass(KG);
  } else {
    to_ret = 0;
  }
  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::ConvertFromKg(double mass, MassUnit to_unit) {
  double converted;
  switch (to_unit) {
    case G :
      converted = mass * 1000.0;
      break;
    case KG :
      converted = mass;
      break;
    default:
      throw Error("The unit provided is not a supported mass unit.");
  }
  return converted;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::ConvertToKg(double mass, MassUnit from_unit) {
  double in_kg;
  switch (from_unit) {
    case G :
      in_kg = mass / 1000.0;
      break;
    case KG :
      in_kg = mass;
      break;
    default:
      throw Error("The unit provided is not a supported mass unit.");
  }
  return in_kg;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::Moles() {
  double m_a_ratio = isoVector().comp()->mass_to_atom_ratio();
  return mass(G) / m_a_ratio;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double Material::Moles(Iso tope) {
  double to_ret;
  CompMapPtr the_comp = isoVector().comp();
  the_comp->Atomify();
  if (the_comp->count(tope) != 0) {
    to_ret = Moles() * isoVector().comp()->AtomFraction(tope);
  } else {
    to_ret = 0;
  }
  return to_ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CompMapPtr Material::UnnormalizeComp(Basis basis, MassUnit unit) {
  CompMapPtr norm_comp = isoVector().comp();
  double scaling;

  switch (basis) {
    case MASS :
      norm_comp->Massify();
      scaling = this->mass(unit);
      break;
    case ATOM :
      norm_comp->Atomify();
      scaling = this->Moles();
      break;
    default :
      throw Error("The basis provided is not a supported CompMap basis");
  }
  CompMapPtr full_comp = CompMapPtr(new CompMap(*norm_comp));
  CompMap::Iterator it;
  for (it = norm_comp->begin(); it != norm_comp->end(); ++it) {
    (*full_comp)[it->first] = scaling * (it->second);
  }

  return full_comp;
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Resource::Ptr Material::clone() {
  CLOG(LEV_DEBUG2) << "Material ID=" << ID_ << " was cloned.";
  Resource::Ptr mat(new Material(*this));
  mat->SetQuantity(this->quantity());
  return mat;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Material::CheckQuality(Resource::Ptr other) {

  // This will be false until proven true
  bool toRet = false;
  IsoVector lhs_vec = iso_vector_;

  try {
    // Make sure the other is a material
    Material::Ptr mat = boost::dynamic_pointer_cast<Material>(other);
    if (mat) {
      toRet = true;
    }
  } catch (...) { }

  CLOG(LEV_DEBUG1) << "Material is checking quality, i.e. both are "
                   << "Materials, and the answer is " << toRet << " with true = " << true << ".";

  return toRet;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Material::Decay() {
  int curr_time = TI->time();
  int delta_time = curr_time - last_update_time_;

  isoVector().Decay(delta_time);
  last_update_time_ = curr_time;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Material::DecayMaterials() {
  for (std::list<Material*>::iterator mat = materials_.begin();
       mat != materials_.end();
       mat++) {
    (*mat)->Decay();
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Material::IsMaterial(Resource::Ptr rsrc) {
  Material::Ptr cast = boost::dynamic_pointer_cast<Material>(rsrc);
  return !(cast.get() == 0);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Material::AddToTable() {
  Resource::AddToTable();
  iso_vector_.Record();
}
} // namespace cyclus