# this makefile requires GNU make.

EXTRA_DIST = algsimp.ml annotate.ml assoctable.ml c.ml complex.ml	\
conv.ml dag.ml expr.ml fft.ml gen_hc2c.ml gen_hc2cdft.ml		\
gen_hc2cdft_c.ml gen_hc2hc.ml gen_r2cb.ml gen_mdct.ml gen_notw.ml	\
gen_notw_c.ml gen_r2cf.ml gen_r2r.ml gen_twiddle.ml gen_twiddle_c.ml	\
gen_twidsq.ml gen_twidsq_c.ml genutil.ml littlesimp.ml magic.ml		\
monads.ml number.ml oracle.ml schedule.ml simd.ml simdmagic.ml		\
to_alist.ml trig.ml twiddle.ml unique.ml util.ml variable.ml		\
algsimp.mli annotate.mli assoctable.mli c.mli complex.mli conv.mli	\
dag.mli expr.mli fft.mli littlesimp.mli number.mli oracle.mli		\
schedule.mli simd.mli to_alist.mli trig.mli twiddle.mli unique.mli	\
util.mli variable.mli

GENFFT_NATIVE=gen_notw.native gen_notw_c.native gen_twiddle.native	\
gen_twiddle_c.native gen_twidsq.native gen_twidsq_c.native		\
gen_r2r.native gen_r2cf.native gen_r2cb.native gen_hc2c.native		\
gen_hc2cdft.native gen_hc2cdft_c.native gen_hc2hc.native		\
gen_mdct.native

all-local::
	$(OCAMLBUILD) -classic-display -libs unix,nums $(GENFFT_NATIVE)

maintainer-clean-local::
	$(OCAMLBUILD) -classic-display -clean
