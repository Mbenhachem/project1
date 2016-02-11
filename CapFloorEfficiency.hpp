/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*!
Copyright (C) 2005, 2006, 2007, 2009 StatPro Italia srl

This file is part of QuantLib, a free-software/open-source library
for financial quantitative analysts and developers - http://quantlib.org/

QuantLib is free software: you can redistribute it and/or modify it
under the terms of the QuantLib license.  You should have received a
copy of the license along with this program; if not, please email
<quantlib-dev@lists.sf.net>. The license is also available online at
<http://quantlib.org/license.shtml>.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

// the only header you need to use QuantLi

#ifdef BOOST_MSVC
/* Uncomment the following lines to unmask floating-point
exceptions. Warning: unpredictable results can arise...

See http://www.wilmott.com/messageview.cfm?catid=10&threadid=9481
Is there anyone with a definitive word about this?
*/
// #include <float.h>
// namespace { unsigned int u = _controlfp(_EM_INEXACT, _MCW_EM); }
#endif



//#include "CapFloorEfficiency.hpp"
#include <boost/foreach.hpp>
#include <boost/timer.hpp>
#include <iostream>
#include <iomanip>
#include "CurveData.hpp"

using namespace QuantLib;

#ifdef BOOST_MSVC
#  ifdef QL_ENABLE_THREAD_SAFE_OBSERVER_PATTERN
#    include <ql/auto_link.hpp>
#    define BOOST_LIB_NAME boost_system
#    include <boost/config/auto_link.hpp>
#    undef BOOST_LIB_NAME
#    define BOOST_LIB_NAME boost_thread
#    include <boost/config/auto_link.hpp>
#    undef BOOST_LIB_NAME
#  endif
#endif


#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {

	Integer sessionId() { return 0; }

}
#endif

namespace {

	struct CapFloorParmeters {
		// common parameters
		Date settlement;
		std::vector<Real> nominals;
		BusinessDayConvention convention;
		Frequency frequency;
		Integer Duration;
		boost::shared_ptr<IborIndex> index;
		Calendar calendar;
		Natural fixingDays;
		RelinkableHandle<YieldTermStructure> termStructure;
		DayCounter termStructureDayCounter;
		Schedule schedule;

		// Some initializations
		CapFloorParmeters() : nominals(1, 100){
			frequency = Semiannual;
			index = boost::shared_ptr<IborIndex>(new Euribor6M(termStructure));
			calendar = TARGET();
			convention = ModifiedFollowing;
			Date today = calendar.adjust(Date::todaysDate());
			Settings::instance().evaluationDate() = today;
			Natural settlementDays = 2;
			fixingDays = 2;
			settlement = calendar.advance(Settings::instance().evaluationDate(), settlementDays, Days);
			termStructureDayCounter = Actual360();

			//  Discountcurve(new InterpolatedDiscountCurve<LogLinear >(dates,forwards,termStructureDayCounter));
			// boost::shared_ptr<YieldTermStructure> flateRate( new FlatForward(settlement, 0.05, termStructureDayCounter, Continuous, frequency) );
			// boost::shared_ptr<YieldTermStructure> flateRate( new InterpolatedForwardCurve<ForwardFlat>(dates,forwards,termStructureDayCounter) );
			// boost::shared_ptr<YieldTermStructure> flateRate( new InterpolatedDiscountCurve <LogLinear>(dates,dfs,termStructureDayCounter) );

			// Constant rate
			//flatRate = FlatForward(settlement, 0.05, termStructureDayCounter, Compounded, frequency) ;



			CurveData cData;
			// Fill out with some sample market data
			sampleMktData(cData);

			// Build a curve linked to this market data
			boost::shared_ptr<YieldTermStructure> ocurve = buildCurve(cData);

			termStructure.linkTo(ocurve);
			Duration = 5;/*
						 Date startDate = termStructure->referenceDate();
						 Date endDate = calendar.advance(startDate, Duration*Years,convention);*/

			Date startDate = calendar.adjust(Date::todaysDate());
			Date endDate = calendar.advance(startDate, Duration*Years, convention);
			schedule = Schedule(startDate, endDate, Period(frequency), calendar,
				convention, convention,
				DateGeneration::Forward, false);
		}

		// Leg defition
		Leg DefinedLeg(const Date& startDate, Integer duration) {
			Date endDate = calendar.advance(startDate, duration*Years, convention);
			Schedule schedule(startDate, endDate, Period(frequency), calendar,
				convention, convention,
				DateGeneration::Forward, false);
			return IborLeg(schedule, index)
				.withNotionals(nominals)
				.withPaymentDayCounter(Actual360())
				.withPaymentAdjustment(convention)
				.withFixingDays(fixingDays);
		}

		// Pricing Engine defition
		boost::shared_ptr<PricingEngine> DefinedEngine(Volatility volatility) {
			Handle<Quote> vol(boost::shared_ptr<Quote>(
				new SimpleQuote(volatility)));
			/*	std::vector <Rate> forwards;
			std::vector <Date > dates;


			InterpolatedForwardCurve <LogLinear > fowardcurve(dates,forwards,termStructureDayCounter);


			boost::shared_ptr<YieldTermStructure> Discountcurve( new InterpolatedDiscountCurve<Linear >(dates,dfs,termStructureDayCounter));
			termStructure.linkTo(Discountcurve);*/
			Real displacement = 0.5;//??
			return boost::shared_ptr<PricingEngine>(
				new BlackCapFloorEngine(termStructure, vol, termStructure->dayCounter(), displacement));
		}

		//Cap floor defintion
		boost::shared_ptr<CapFloor> DefinedCapFloor(CapFloor::Type type, const Leg& leg, const std::vector<Rate>& strike, Volatility volatility)
		{
			boost::shared_ptr<CapFloor> result;
			switch (type) {
			case CapFloor::Cap:
				result = boost::shared_ptr<CapFloor>(
					new Cap(leg, strike));
				break;
			case CapFloor::Floor:
				result = boost::shared_ptr<CapFloor>(
					new Floor(leg, strike));
				break;
			default:
				QL_FAIL("unknown cap/floor type");
			}
			result->setPricingEngine(DefinedEngine(volatility));
			return result;
		}

	};

	std::string typeToString(CapFloor::Type type) {
		switch (type) {
		case CapFloor::Cap:
			return "cap";
		case CapFloor::Floor:
			return "floor";
		case CapFloor::Collar:
			return "collar";
		default:
			QL_FAIL("unknown cap/floor type");
		}
	}

}



void PrintCurve(CapFloorParmeters &capfloorEfficiecy){

	std::vector <Date > SchedulDates = capfloorEfficiecy.schedule.dates();

	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;
	Calendar  calendar = TARGET();

	std::cout << " -------------------------MARKET DATA  -------------------------            |" << std::endl;
	std::cout << std::endl;

	std::cout << "Start              |" << "End              |" << "Fixing              |" << "IDays              |"  "Date              |" << "Zero Rate:        |" << "Discount Rate       |" << "Forward Rate        |" << "EURIBOR Rate        |" << std::endl;
	BOOST_FOREACH(Date d, SchedulDates){
		DayCounter dc = Actual360();
		Integer fixing = 2;
		Date FixingDate = calendar.advance(d, -fixing, Days);
		Date startDate = capfloorEfficiecy.schedule.previousDate(d);
		if (startDate != Date()) {
			Date endDate = capfloorEfficiecy.schedule.nextDate(d);
			Time Idays = daysBetween(startDate, endDate);
			Date d1 = d + 6 * Months;
			std::cout << io::short_date(startDate) << "            " <<
				io::short_date(endDate) << "            " <<
				io::short_date(FixingDate) << "            " <<
				Idays << "            " <<
				io::short_date(d) << "            " <<
				100 * capfloorEfficiecy.termStructure->zeroRate(FixingDate, dc, Compounded).rate() << "            " <<
				capfloorEfficiecy.termStructure->discount(FixingDate) << "            " <<
				100 * capfloorEfficiecy.termStructure->forwardRate(FixingDate, d1, dc, Compounded).rate() << "         " <<
				100 * capfloorEfficiecy.index->fixing(FixingDate) << "         " << std::endl;
		}
	}
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;

}

void PrintResults(const std::vector<Rate>& strikes, Volatility vol, Real nominal, Date maturityDate, Date startDate, boost::shared_ptr<CapFloor> InstrumentEfficiency, CapFloor::Type InstrumentType, CapFloorParmeters &capfloorEfficiecy){
	std::vector <Date > SchedulDates = capfloorEfficiecy.schedule.dates();
	Size length = SchedulDates.size();
	std::cout << std::endl;
	std::cout << std::endl;
	Date MDate;
	
	std::vector<boost::shared_ptr<CapFloor> > caplets;
	if (InstrumentType == CapFloor::Cap) {
		std::cout << "Dates              |" << "" << "NPV Caplet  |" << " Strike of Caplet |" << "  Floating Rate coupon  |" << " Index Rate| " << " Strike " << std::endl;
		for (Size i = 0; i< length - 1; i++) {
			caplets.push_back(InstrumentEfficiency->optionlet(i));
			MDate = caplets[i]->maturityDate();
			caplets[i]->setPricingEngine(capfloorEfficiecy.DefinedEngine(vol));

			std::cout << io::short_date(MDate) << "          "
				<< caplets[i]->NPV() << "         "
				<< io::rate(caplets[i]->capRates()[0]) << "      " <<
				caplets[i]->lastFloatingRateCoupon()->amount() << "     " <<
				io::rate(capfloorEfficiecy.index->fixing(MDate)) 
				<< strikes[i] << std::endl;
		}
	}
	else if (InstrumentType == CapFloor::Floor){
		std::cout << "Dates              |" << "" << "NPV Floorlet  |" << " Strike of Floorlet |" << " Floating Rate coupon  |" << " Index Rate " << std::endl;
		for (Size i = 0; i< length - 1; i++) {
			caplets.push_back(InstrumentEfficiency->optionlet(i));
			MDate = caplets[i]->maturityDate();
			caplets[i]->setPricingEngine(capfloorEfficiecy.DefinedEngine(vol));

			std::cout << io::short_date(MDate) << "          "
				<< caplets[i]->NPV() << "         "
				<< io::rate(caplets[i]->floorRates()[0]) << "      " <<
				caplets[i]->lastFloatingRateCoupon()->amount() << "     " <<
				io::rate(capfloorEfficiecy.index->forecastFixing(MDate)) << std::endl;
		}
	}
	else{ QL_FAIL("unknown cap/floor type"); }

	//Leg leg = InstrumentEfficiency->floatingLeg();
	//boost::shared_ptr<FloatingRateCoupon> FloRateCoupon = InstrumentEfficiency->lastFloatingRateCoupon();
	//	std::cout << "Dates              |" << "            "  << "Cash Flow     |" << std::endl; 
	//BOOST_FOREACH(auto cashF, leg){
	//	
	//	Date d = cashF->date();
	//	Real Amount = capfloorEfficiecy.termStructure->discount(d)*cashF->amount();
	//	std::cout <<d   << "              "  << Amount << std::endl; 
	//}
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << "---------------------------------------------------|" << std::endl;
	std::cout << " Strike = " << io::rate(strikes[0]) << std::endl;
	std::cout << " Volatility = " << io::volatility(vol) << std::endl;
	std::cout << " Notional = " << nominal << std::endl;
	std::cout << " Start Date = " << io::short_date(startDate) << std::endl;
	std::cout << " End Date = " << io::short_date(maturityDate) << std::endl;
	std::cout << " Option type = " << InstrumentType << "......";
	std::cout << " NPV = " << InstrumentEfficiency->NPV() << std::endl;

	std::cout << std::endl;
	std::cout << "----------------------------------------------------|" << std::endl;
	std::cout << std::endl;
	std::cout << " Taper 1 pour continuer ,  0 pour Quitter  : ";
}
void Inputparmeters(CapFloor::Type & InstrumentType, Real & strikes, Real & vol, Real & notional){

	std::string s;
	std::string const cap = "1";
	std::string const floor = "2";
	std::cout << " Choisir un produit : 1: Cap, 2: Floor ";
	std::cin >> s;
	if (s == cap)
		InstrumentType = CapFloor::Cap;
	else if (s == floor)
		InstrumentType = CapFloor::Floor;
	else
		QL_FAIL("unknown cap/floor produit");

	std::cout << " Entrer le  Strike  ";
	std::cin >> s;
	try{ strikes = boost::lexical_cast<Real>(s); }
	catch (boost::bad_lexical_cast const&)
	{
		strikes = 0;
	}

	while (strikes == 0) {
		std::cout << " Entrer le  Strike   ";
		std::cin >> s;
		try
		{
			strikes = boost::lexical_cast<Real>(s);
		}
		catch (boost::bad_lexical_cast const&)
		{
			strikes = 0;
		}
	}

	std::cout << " Entrer la volatilite   ";
	std::cin >> s;
	try{ vol = boost::lexical_cast<Real>(s); }
	catch (boost::bad_lexical_cast const&)
	{
		vol = 0;
	}
	while (vol == 0) {
		std::cout << " Entrer la  volatilite  ";
		std::cin >> s;
		try
		{
			vol = boost::lexical_cast<Real>(s);
		}
		catch (boost::bad_lexical_cast const&)
		{
			vol = 0;
		}
	}
	std::cout << " Entrer le  nominal   ";
	std::cin >> s;
	try{ notional = boost::lexical_cast<Real>(s); }
	catch (boost::bad_lexical_cast const&)
	{
		notional = 0;
	}

	while (notional == 0) {
		std::cout << " Entrer le  nominal   ";
		std::cin >> s;
		try
		{
			notional = boost::lexical_cast<Real>(s);
		}
		catch (boost::bad_lexical_cast const&)
		{
			notional = 0;
		}
	}
	std::cout << std::endl;

}

void TestCapFloor(){
	// Parmameters
	CapFloorParmeters capfloorEfficiecy;
	Integer Duration = 5;
	Volatility vol = 0.15;
	Rate strike = 0.1;
	Calendar calendar = TARGET();
	Date startDate = calendar.adjust(Date::todaysDate());
	Real notional = 100;
	capfloorEfficiecy.nominals[0] = notional;
	//Deifne Leg
	Leg leg = capfloorEfficiecy.DefinedLeg(startDate, Duration);
	std::string s;
	int Fin = 1;
	std::vector<Rate> strikes(10);


	for (int i = 0; i <= 9; i++)
	{
		strikes[i] = 0.1 + 0.001*i*pow(-1, i);
	}
	/*std::cout << " Entrer de nouveaux parametres " << std::endl;
	std::cout << " Taper 1 pour continuer ,  0 pour Quitter   : " ;
	std::cin >> s ;
	try
	{Fin = boost::lexical_cast<int>(s);}
	catch (boost::bad_lexical_cast const&)
	{Fin = 0;}*/
	std::istringstream convert;
	CapFloor::Type InstrumentType;
	PrintCurve(capfloorEfficiecy);
	while (Fin != 0) {
		Inputparmeters(InstrumentType, strike, vol, notional);
		capfloorEfficiecy.nominals[0] = notional;
		//Define the product

		boost::shared_ptr<CapFloor> InstrumentEfficiency =
			capfloorEfficiecy.DefinedCapFloor(InstrumentType, leg, strikes, vol);

		// Print rseults
		PrintResults(strikes, vol, capfloorEfficiecy.nominals[0], InstrumentEfficiency->maturityDate(), startDate, InstrumentEfficiency, InstrumentType, capfloorEfficiecy);
		std::cin >> s;
		try
		{
			Fin = boost::lexical_cast<int>(s);
		}
		catch (boost::bad_lexical_cast const&)
		{
			Fin = 0;
		}
	}
}

