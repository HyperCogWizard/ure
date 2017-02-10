/*
 * Rule.cc
 *
 * Copyright (C) 2015 OpenCog Foundation
 *
 * Authors: Misgana Bayetta <misgana.bayetta@gmail.com> 2015
 *          Nil Geisweiller 2015-2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <queue>

#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/base/Quotation.h>
#include <opencog/atoms/core/DefineLink.h>
#include <opencog/atoms/pattern/BindLink.h>

#include <opencog/atomutils/TypeUtils.h>
#include <opencog/atomutils/Unify.h>

#include <opencog/query/BindLinkAPI.h>

#include "Rule.h"

namespace opencog {

void RuleSet::expand_meta_rules(AtomSpace& as)
{
	for (const Rule& rule : *this) {
		if (rule.is_meta()) {
			Handle result = rule.apply(as);
			for (const Handle& produced_h : result->getOutgoingSet()) {
				Rule produced(rule.get_alias(), produced_h, rule.get_rbs());
				this->insert(produced);
			}
		}
	}
}

Rule::Rule()
	: _rule_alias(Handle::UNDEFINED) {}

Rule::Rule(const Handle& rule_member)
{
	init(rule_member);
}

Rule::Rule(const Handle& rule_alias, const Handle& rbs)
{
	init(rule_alias, rbs);
}

Rule::Rule(const Handle& rule_alias, const Handle& rule, const Handle& rbs)
{
	init(rule_alias, rule, rbs);
}

void Rule::init(const Handle& rule_member)
{
	OC_ASSERT(rule_member != Handle::UNDEFINED);
	if (not classserver().isA(rule_member->getType(), MEMBER_LINK))
		throw InvalidParamException(TRACE_INFO,
		                            "Rule '%s' is expected to be a MemberLink",
		                            rule_member->toString().c_str());

	Handle rule_alias = rule_member->getOutgoingAtom(0);
	Handle rbs = rule_member->getOutgoingAtom(1);
	init(rule_alias, rbs);
}

void Rule::init(const Handle& rule_alias, const Handle& rbs)
{
	Handle rule = DefineLink::get_definition(rule_alias);
	init(rule_alias, rule, rbs);
}

void Rule::init(const Handle& rule_alias, const Handle& rule, const Handle& rbs)
{
	OC_ASSERT(rule->getType() == BIND_LINK);
	_rule = BindLinkCast(rule);

	_rule_alias = rule_alias;
	_name = _rule_alias->getName();
	_rbs = rbs;
	AtomSpace& as = *rule_alias->getAtomSpace();
	Handle ml = as.get_link(MEMBER_LINK, rule_alias, rbs);
	_weight = ml->getTruthValue()->getMean();
}

bool Rule::operator==(const Rule& r) const
{
	return r._rule == _rule;
}

bool Rule::operator<(const Rule& r) const
{
	return _weight == r._weight ?
		Handle(_rule).value() < Handle(r._rule).value()
		: _weight < r._weight;
}

bool Rule::is_alpha_equivalent(const Rule& r) const
{
	return _rule->is_equal(Handle(r._rule));
}

double Rule::get_weight() const
{
	return _weight;
}

void Rule::set_name(const std::string& name)
{
	_name = name;
}

std::string& Rule::get_name()
{
	return _name;
}

const std::string& Rule::get_name() const
{
	return _name;
}

void Rule::set_rule(const Handle& h)
{
	_rule = BindLinkCast(h);
}

Handle Rule::get_rule() const
{
	return Handle(_rule);
}

Handle Rule::get_alias() const
{
	return _rule_alias;
}

Handle Rule::get_rbs() const
{
	return _rbs;
}

void Rule::add(AtomSpace& as)
{
	if (!_rule)
		return;

	HandleSeq outgoings;
	for (const Handle& h : _rule->getOutgoingSet())
		outgoings.push_back(as.add_atom(h));
	_rule = createBindLink(outgoings);
}

Handle Rule::get_vardecl() const
{
	if (_rule)
		return _rule->get_vardecl();
	return Handle::UNDEFINED;
}

/**
 * Get the implicant (input) of the rule defined in a BindLink.
 *
 * @return the Handle of the implicant
 */
Handle Rule::get_implicant() const
{
	if (_rule)
		return _rule->get_body();
	return Handle::UNDEFINED;
}

Handle Rule::get_implicand() const
{
	if (_rule)
		return _rule->get_implicand();
	return Handle::UNDEFINED;
}

bool Rule::is_valid() const
{
	return (bool)_rule;
}

bool Rule::is_meta() const
{
	Handle implicand = get_implicand();

	if (implicand.is_undefined())
		return false;

	Type itype = implicand->getType();
	return (Quotation::is_quotation_type(itype) ?
	        implicand->getOutgoingAtom(0)->getType() : itype) == BIND_LINK;
}

/**
 * Get the set of members of the implicant which are
 * connected by a root logical link.
 *
 * @return HandleSeq of members of the implicant
 */
HandleSeq Rule::get_clauses() const
{
	// if the rule's handle has not been set yet
	if (!_rule)
		return HandleSeq();

    Handle implicant = get_implicant();
    Type t = implicant->getType();
    HandleSeq hs;

    if (t == AND_LINK or t == OR_LINK)
        hs = implicant->getOutgoingSet();
    else
        hs.push_back(implicant);

    return hs;
}

/**
 * Get the conclusion (output) of the rule.  defined in a BindLink.
 *
 * @return the Handle of the implicand
 *
 * TODO: indentical to get_implicand()
 */
Handle Rule::get_conclusion() const
{
	if (_rule)
		return _rule->get_implicand();
	return Handle::UNDEFINED;
}

HandlePairSeq Rule::get_conclusions() const
{
	HandlePairSeq results;

	// If the rule's handle has not been set yet
	if (!_rule)
		return HandlePairSeq();

	Handle vardecl = get_vardecl();
	for (const Handle& c : get_conclusion_patterns())
		results.push_back({filter_vardecl(vardecl, c), c});

	return results;
}

void Rule::set_weight(double p)
{
	_weight = p;
}

Rule Rule::gen_standardize_apart(AtomSpace* as)
{
	if (!_rule)
		throw InvalidParamException(TRACE_INFO,
		                            "Attempted standardized-apart on "
		                            "invalid Rule");

	// clone the Rule
	Rule st_ver = *this;
	HandleMap dict;

	Handle vdecl = get_vardecl();
	OrderedHandleSet varset;

	if (VariableListCast(vdecl))
		varset = VariableListCast(vdecl)->get_variables().varset;
	else
		varset.insert(vdecl);

	for (auto& h : varset)
		dict[h] = Handle::UNDEFINED;

	Handle st_bindlink = standardize_helper(as, Handle(_rule), dict);
	st_ver.set_rule(st_bindlink);

	return st_ver;
}

RuleSet Rule::unify_source(const Handle& source,
                           const Handle& vardecl) const
{
	// TODO
	return {};
}

RuleSet Rule::unify_target(const Handle& target,
                           const Handle& vardecl) const
{
	// If the rule's handle has not been set yet
	if (!_rule)
		return {};

	Rule alpha_rule = rand_alpha_converted();

	logger().debug() << "Rule::unify_target" << std::endl
	                 << "alpha_rule:" << std::endl << oc_to_string(alpha_rule);

	RuleSet unified_rules;

	Handle alpha_vardecl = alpha_rule.get_vardecl();
	for (const Handle& alpha_pat : alpha_rule.get_conclusion_patterns())
	{
		logger().debug() << "Rule::unify_target" << std::endl
		                 << "target:" << std::endl << target
		                 << "alpha_pat:" << std::endl << alpha_pat
		                 << "vardecl:" << std::endl << vardecl
		                 << "alpha_vardecl:" << std::endl << alpha_vardecl;
		Unify::SolutionSet sol =
			Unify()(target, alpha_pat, vardecl, alpha_vardecl);
		logger().debug() << "Rule::unify_target" << std::endl
		                 << "sol:" << std::endl << oc_to_string(sol);
		if (sol.satisfiable) {
			Unify::TypedSubstitutions tss =
				Unify().typed_substitutions(sol, target, target, alpha_pat,
				                            vardecl, alpha_vardecl);
			// For each typed substitution produce a new rule by
			// substituting all variables by their associated
			// values.
			for (const auto& ts : tss)
				unified_rules.insert(alpha_rule.substituted(ts));
		}
	}

	return unified_rules;
}

Handle Rule::apply(AtomSpace& as) const
{
	return bindlink(&as, Handle(_rule));
}

std::string Rule::to_string() const
{
	std::stringstream ss;
	ss << "name: " << _name << std::endl
	   << "rule:" << std::endl << _rule->toString();
	return ss.str();
}

Rule Rule::rand_alpha_converted() const
{
	// Clone the rule
	Rule result = *this;

	// Alpha convert the rule
	result.set_rule(_rule->alpha_conversion());

	return result;
}

/**
 * Basic helper function to standardize apart the BindLink.
 *
 * @param as     pointer to an atomspace where new atoms are added
 * @param h      an input atom to standardize apart
 * @param dict   a mapping of old VariableNode and new VariableNode
 * @return       the new atom
 */
Handle Rule::standardize_helper(AtomSpace* as, const Handle& h,
                                HandleMap& dict)
{
	if (h->isLink())
	{
		HandleSeq old_outgoing = h->getOutgoingSet();
		HandleSeq new_outgoing;

		for (auto ho : old_outgoing)
			new_outgoing.push_back(standardize_helper(as, ho, dict));

		return as->add_atom(createLink(h->getType(), new_outgoing,
		                               h->getTruthValue()));
	}

	// normal node does not need to be changed
	if (h->getType() != VARIABLE_NODE)
		return h;

	// If the VariableNode is not scoped by the rule's scope, but is
	// instead scoped by something generated by the output, then we
	// want to generate a completely unique variable
	if (dict.count(h) == 0)
	{
		// TODO: use opencog's random generator
		std::string new_name = h->getName() + "-"
			+ boost::uuids::to_string(boost::uuids::random_generator()());
		Handle hcpy = as->add_atom(createNode(h->getType(), new_name,
		                                      h->getTruthValue()));

		dict[h] = hcpy;

		return hcpy;
	}

	// use existing mapping if the VariableNode is already mapped
	if (dict.at(h) != Handle::UNDEFINED)
		return dict[h];

	std::string new_name = h->getName() + "-" + _name;
	Handle hcpy = as->add_atom(createNode(h->getType(), new_name,
	                                      h->getTruthValue()));

	dict[h] = hcpy;

	return hcpy;
}

HandleSeq Rule::get_conclusion_patterns() const
{
	HandleSeq results;
	Handle implicand = get_implicand();
	Type t = implicand->getType();
	if (LIST_LINK == t)
		for (const Handle& h : implicand->getOutgoingSet())
			results.push_back(get_conclusion_pattern(h));
	else
		results.push_back(get_conclusion_pattern(implicand));

	return results;
}

Handle Rule::get_conclusion_pattern(const Handle& h) const
{
	Type t = h->getType();
	if (EXECUTION_OUTPUT_LINK == t)
		return get_execution_output_first_argument(h);
	else
		return h;
}

Handle Rule::get_execution_output_first_argument(const Handle& h) const
{
	OC_ASSERT(h->getType() == EXECUTION_OUTPUT_LINK);
	Handle args = h->getOutgoingAtom(1);
	if (args->getType() == LIST_LINK) {
		OC_ASSERT(args->getArity() > 0);
		return args->getOutgoingAtom(0);
	} else
		return args;
}

Rule Rule::substituted(const Unify::TypedSubstitution& ts) const
{
	Rule new_rule(*this);
	new_rule.set_rule(Unify().substitute(_rule, ts));
	return new_rule;
}

std::string oc_to_string(const Rule& rule)
{
	return rule.to_string();
}

std::string oc_to_string(const RuleSet& rules)
{
	std::stringstream ss;
	ss << "size = " << rules.size() << std::endl;
	size_t i = 0;
	for (const Rule& rule : rules)
		ss << "rule[" << i++ << "]:" << std::endl
		   << oc_to_string(rule) << std::endl;
	return ss.str();
}

} // ~namespace opencog
