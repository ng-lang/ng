Should compile hir program
  $ ngc  ../../example/hir.ng
  val m = 1;
  .define print/1:
    val x = arguments[0];
    x;
  .end_define print
  .define invoke/1:
    (1,2,3,4);
  .end_define invoke
  print(m);
  val _V02 = invoke(unit);
  val a = _V02[0];
  val b = _V02[1];
  val c = _V02[2];
  val d = _V02[3];
  (a + 1);
  print(1);
  val _V05 = ((a + b),b);
  val x = _V05[0];
  val y = _V05[1];
  .define hello/1:
    val _0I06 = arguments[0];
    val x = _0I06[0];
    print(x);
  .end_define hello
  .define fn/5:
    val a = arguments[0];
    val b = arguments[1];
    val _2I07 = arguments[2];
    val c = _2I07[0];
    val d = _2I07[1];
    val e = _2I07[2];
    val _3I08 = arguments[3];
    val f = _3I08[0];
    val g = _3I08[1];
    val _4I09 = arguments[4];
    val h = _4I09[0];
    val i = _4I09[1];
    [a,b,c,d,e,f,g,h,sizeof(i)];
  .end_define fn
  .define test_statement_body/1:
    val _0I0a = arguments[0];
    val x = _0I0a[0];
    val y = (x + 1);
    ret (x + y);
  .end_define test_statement_body


