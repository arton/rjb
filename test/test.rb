#!/usr/local/env ruby
# $Id$

require 'test/unit'
require 'rjb'

puts "start RJB(#{Rjb::VERSION}) test"
class TestRjb < Test::Unit::TestCase
  include Rjb
  def setup
    load('.')
    Rjb::primitive_conversion = false
  end

  def tearDown
    unload
  end

  def test_metaclass
    cls = import('java.lang.Class')
    assert_equal('java.lang.Class', cls._classname)
    assert_equal('java.lang.Class', cls.getName)
    assert_equal(17, cls.getModifiers)
  end

  def test_scalar
    cls = import('java.lang.String')
    assert_equal('java.lang.Class', cls._classname)
    assert_equal('class java.lang.String', cls.toString)
    str = cls.new
    assert_equal('java.lang.String', str._classname)
    assert_equal(0, str.length)
    assert_equal('', str.toString)
    str = cls.new_with_sig('Ljava.lang.String;', "abcde")
    # result integer
    assert_equal(5, str.length)
    # result string
    assert_equal('abcde', str.toString)
    # argument test
    # char
    assert_equal('abxde', str.replace(?c, ?x))
    # string
    assert_equal('abdxe', str.replaceAll('cd', 'dx'))
    # int
    assert_equal('bc', str.substring(1, 3))
    assert_equal('e', str.substring(4))
    # float with static
    assert_equal('5.23', cls._invoke('valueOf', 'F', 5.23))
    assert_equal('25.233', cls._invoke('valueOf', 'D', 25.233))
    # rjb object (String)
    str2 = cls.new_with_sig('Ljava.lang.String;', 'fghijk')
    assert_equal('abcdefghijk', str.concat(str2))
    # rjb object other (implicit toString call is Rjb feature)
    jint = import('java.lang.Integer')
    i = jint.new_with_sig('I', 35901)
    assert_equal('abcde35901', str.concat(i))
    # result boolean and argument is rjb object
    assert_equal(false, i.equals(str))
    assert_equal(false, str.equals(i))
    assert_equal(true, str.equals("abcde"))
    assert_equal(true, str.equals(str))
    # long
    lng = import('java.lang.Long')
    l = lng.new_with_sig('J', -9223372036854775808)
    assert_equal(-9223372036854775808, l.longValue)
    l = lng.new_with_sig('J', 9223372036854775807)
    assert_equal(9223372036854775807, l.longValue)
    # double
    dbl = import('java.lang.Double')
    d = dbl.new_with_sig('D', 1234.5678901234567890)
    assert_equal(1234.5678901234567890, d.doubleValue)
    # byte
    byt = import('java.lang.Byte')
    b = byt.new_with_sig('B', 13)
    assert_equal(13, b.byteValue)
    # float
    flt = import('java.lang.Float')
    f = flt.new_with_sig('F', 13.5)
    assert_equal(13.5, f.floatValue)
    # short
    sht = import('java.lang.Short')
    s = sht.new_with_sig('S', 1532)
    assert_equal(1532, s.shortValue)
    chr = import('java.lang.Character')
    c = chr.new_with_sig('C', ?A)
    assert_equal(?A, c.charValue)
  end

  def test_array
    cls = import('java.lang.String')
    str = cls.new_with_sig('[C', [?a, ?b, ?c, ?d, ?e, ?c, ?f, ?c, ?g])
    assert_equal('abcdecfcg', str.toString)
    # conv string array
    splt = str.split('c')
    assert(Array === splt)
    assert_equal(4, splt.size)
    assert_equal('ab', splt[0])
    assert_equal('g', splt[3])
    # conv byte array to (ruby)string
    ba = str.getBytes
    assert_equal('abcdecfcg', ba)
    # conv char array to array(int)
    ca = str.toCharArray
    assert_equal([?a, ?b, ?c, ?d, ?e, ?c, ?f, ?c, ?g], ca)
  end

  def test_importobj()
    sys = import('java.lang.System')
    props = sys.getProperties
    assert_equal('java.util.Properties', props._classname)
    if /cygwin/ =~ RUBY_PLATFORM # patch for dirty environment
      assert_equal(Dir::pwd, %x[cygpath -u #{props.getProperty('user.dir').gsub('\\', '/')}].chop)
    else
      assert_equal(Dir::pwd, props.getProperty('user.dir').gsub('\\', '/'))
    end
    boolean = import('java.lang.Boolean')
    assert_equal(boolean.valueOf(true).booleanValue(), true)
    assert_equal(boolean.valueOf(false).booleanValue(), false)
    assert_equal(boolean.valueOf('true').booleanValue(), true)
    assert_equal(boolean.valueOf('false').booleanValue(), false)
  end

  def test_importobjarray()
    jarray = import('java.util.ArrayList')
    a = jarray.new()
    jint = import('java.lang.Integer')
    a.add(jint.new_with_sig('I', 1))
    a.add(jint.new_with_sig('I', 2))
    a.add(jint.new_with_sig('I', 3))
    oa = a.toArray
    assert_equal(3, oa.size)
    assert_equal(1, oa[0].intValue)
    assert_equal(2, oa[1].intValue)    
    assert_equal(3, oa[2].intValue)    
  end

  def test_kjconv()
    $KCODE = 'euc'
    cls = import('java.lang.String')
    euc_kj = "\xb4\xc1\xbb\xfa\xa5\xc6\xa5\xad\xa5\xb9\xa5\xc8"
    s = cls.new(euc_kj)
    assert_equal(s.toString(), euc_kj)
    $KCODE = 'sjis'
    sjis_kj = "\x8a\xbf\x8e\x9a\x83\x65\x83\x4c\x83\x58\x83\x67"
    s = cls.new(sjis_kj)
    assert_equal(s.toString(), sjis_kj)
  end

  def test_constants()
    lng = import('java.lang.Long')
    assert_equal(0x7fffffffffffffff, lng.MAX_VALUE)
    assert_equal(-9223372036854775808, lng.MIN_VALUE)
  end

  class TestIter
    def initialize()
      @i = 5
    end
    def hasNext()
      @i > 0
    end
    def next()
      @i -= 1
      @i.to_s
    end
  end

  def test_newobject()
    it = TestIter.new
    it = bind(it, 'java.util.Iterator')
    test = import('jp.co.infoseek.hp.arton.rjb.Test')
    a = test.new
    assert("43210", a.concat(it))
  end

  class TestComparator
    def compare(o1, o2)
      o1.to_i - o2.to_i
    end
    def equals(o)
      o == self
    end
  end

  def test_comparator
    cp = TestComparator.new
    cp = bind(cp, 'java.util.Comparator')
    test = import('jp.co.infoseek.hp.arton.rjb.Test')
    a = test.new
    assert(0, a.check(cp, 123, 123))
    assert(5, a.check(cp, 81, 76))
    assert(-5, a.check(cp, 76, 81))
  end

  # assert_raise is useless in this test, because NumberFormatException may be defined in
  # its block.
  def test_exception()
    it = import('java.lang.Integer')
    begin
      it.parseInt('blabla')
      flunk('no exception')
    rescue NumberFormatException => e
      # OK
    end
  end

  class TestIterator
    def initialize(tp)
      @type = tp
    end
    def hasNext()
      true
    end
    def next()
      if @type == 0
	Rjb::throw('java.util.NoSuchElementException', 'test exception')
      elsif @type == 1
	Rjb::throw(Rjb::import('java.util.NoSuchElementException').new('instance test'))
      end
    end
  end

  def test_throw()
    it = TestIterator.new(0)
    it = bind(it, 'java.util.Iterator')
    test = import('jp.co.infoseek.hp.arton.rjb.Test')
    a = test.new
    begin
      a.concat(it)
      flunk('no exception')
    rescue NoSuchElementException => e
      assert_equal('test exception', e.message)
    end
  end

  def test_instance_throw()
    it = TestIterator.new(1)
    it = bind(it, 'java.util.Iterator')
    test = import('jp.co.infoseek.hp.arton.rjb.Test')
    a = test.new
    begin
      a.concat(it)
      flunk('no exception')
    rescue NoSuchElementException => e
      assert_equal('instance test', e.message)
    end
  end

  def test_null_string()
    sys = import('java.lang.System')
    begin
      sys.getProperty(nil)
      flunk('no exception')
    rescue NullPointerException => e
      assert(true)
    rescue RuntimeError => e
      flunk(e.message)
    end
  end

  def test_throw_error()
    begin
      throw(self)
      flunk('no exception')
    rescue TypeError => e
    end
    begin
      throw(import('java.lang.String').new('a'))
      flunk('no exception')
    rescue RuntimeError => e
      assert_equal('arg1 must be a throwable', e.message)
    end
    begin
      throw('java.lang.NoSuchException', 'test')
      flunk('no excpetion')
    rescue RuntimeError => e
      assert_equal("`java.lang.NoSuchException' not found", e.message)
    end
  end

  def test_field()
    point = import('java.awt.Point')
    pnt = point.new(11, 13)
    assert_equal(11, pnt.x)
    assert_equal(13, pnt.y)
    pnt.x = 32
    assert_equal(32, pnt.x)
  end

  def test_instancemethod_from_class()
    begin
      cls = import('java.lang.String')
      assert_equal('true', cls.valueOf(true))
      cls.length
      flunk('no exception')
    rescue RuntimeError => e
      assert_equal('instance method `length\' for class', e.message)
    end
  end

  def test_instancefield_from_class()
    point = import('java.awt.Point')
    begin
      point.x
      flunk('no exception')
    rescue RuntimeError => e
      assert_equal('instance field `x\' for class', e.message)
    end
    begin
      point.x = 30
    rescue RuntimeError => e
      assert_equal('instance field `x\' for class', e.message)
    end
  end

  def test_static_derived_method()
    ext = import('jp.co.infoseek.hp.arton.rjb.ExtBase')
    assert_equal("sVal", ext.getSVal)
  end

  def test_capitalized_method()
    bs = import('jp.co.infoseek.hp.arton.rjb.Base')
    assert_equal("val", bs.val)
    assert_equal("Val", bs.Val)
  end

  def test_underscored_constant()
    bs = import('jp.co.infoseek.hp.arton.rjb.Base')
    assert_equal(5, bs._NUMBER_FIVE)
  end

  def test_passingclass()
    ibs = import('jp.co.infoseek.hp.arton.rjb.IBase')
    bs = import('jp.co.infoseek.hp.arton.rjb.Base')
    assert_equal('interface jp.co.infoseek.hp.arton.rjb.IBase', bs.intf(ibs))
  end

  def test_fornamehook()
    # j2se class
    cls = import('java.lang.Class')
    c = cls.forName('java.lang.Class')
    assert_equal(cls, c)
    # user class
    bs = import('jp.co.infoseek.hp.arton.rjb.Base')
    b = cls.forName('jp.co.infoseek.hp.arton.rjb.Base')
    assert_equal(bs, b)
    # class java's Class#forName and convert result to imported class
    loader = Rjb::import('java.lang.ClassLoader')
    b = cls.forName('jp.co.infoseek.hp.arton.rjb.Base', true, loader.getSystemClassLoader)
    assert_equal(bs, b)
    b = cls.forName('jp.co.infoseek.hp.arton.rjb.IBase', true, loader.getSystemClassLoader)
    assert(b.isInterface)
  end

  def test_send_array_of_arrays()
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    a = test.joinStringArray([['ab', 'cd'], ['ef', 'gh']])
    assert_equal(['ab', 'cd', 'ef', 'gh'], a)
    a = test.joinIntArray([[1, 2, 3], [4, 5, 6]])
    a.collect! {|e| e.intValue }
    assert_equal([1, 2, 3, 4, 5, 6], a)
    r = [[[ 1, 2], [2, 3] ], [[ 3, 4], [5, 6]], [[7, 8], [1, 3]]]
    a = test.throughIntArray(r)
    assert_equal(a, r)
  end

  def test_import_and_instanciate()
    b = import('jp.co.infoseek.hp.arton.rjb.Base')
    assert_equal('hello', b.new.getInstanceVar())
  end

  def test_array_of_arrays()
    jversion = import('java.lang.System').getProperty('java.version')
    if /^1\.5/ =~ jversion
      method = import('java.lang.reflect.Method')
    end
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    a = test.getStringArrayOfArrays()
    assert_equal("abc", a[0][0])
    assert_equal("def", a[0][1])
    assert_equal("123", a[1][0])
    assert_equal("456", a[1][1])

    ints = test.getIntArrayOfArrays()
    assert_equal(2, ints.size )
    assert_equal([1,2,3], ints[0] )
    assert_equal([[1,2,3],[4,5,6]], ints )

    sized = test.getSizedArray()
    assert_equal("find me",sized[0][1][2][3])

    mixed = test.getMixedArray()
    assert_equal(12,mixed[0][0][0].intValue)
    assert_equal("another string",mixed[1][0][1].toString)
    assert_equal([],mixed[2])
  end

  def test_CastObjectArray()
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    a = test.getObjectArray()
    assert_equal(1, a[0].intValue)
    assert_equal('Hello World !', a[1].toString)
    a = test.getObjectArrayOfArray()
    assert_equal(1, a[0][0].intValue)
    assert_equal('Hello World !', a[0][1].toString)
    assert_equal(2, a[1][0].intValue)
    assert_equal('Hello World !!', a[1][1].toString)
  end

  def test_CallByNullForArraies()
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    assert_equal(nil, test.callWithArraies(nil, nil, nil, nil, nil, nil,
                                           nil, nil))
  end

  def test_failed_constructor_call()
    test = import('java.lang.String')
    begin
      s = test.new('a', 'b', 'c')
      flunk('no exception')
    rescue RuntimeError => e
      assert(e)
    end
  end

  def test_rubyize
    loader = Rjb::import('java.lang.ClassLoader')
    cls = import('java.lang.Class')
    b = cls.for_name('jp.co.infoseek.hp.arton.rjb.IBase', true, loader.system_class_loader)
    assert(b.interface?)
    stringbuffer = Rjb::import('java.lang.StringBuffer')
    sb = stringbuffer.new('abc')
    assert_equal(1, sb.index_of('bc'))
    sb.set_char_at(1, ?B)
    assert_equal('aBc', sb.to_string)
    sb.length = 2
    assert_equal('aB', sb.to_string)
  end

  def test_auto_conv
    assert_equal(false, Rjb::primitive_conversion)
    Rjb::primitive_conversion = true
    assert_equal(true, Rjb::primitive_conversion)
    jInteger = Rjb::import('java.lang.Integer')
    assert_equal(1, jInteger.valueOf('1'))
    assert_equal(-1, jInteger.valueOf('-1'))
    jShort = Rjb::import('java.lang.Short')
    assert_equal(2, jShort.valueOf('2'))
    assert_equal(-2, jShort.valueOf('-2'))
    jDouble = Rjb::import('java.lang.Double')
    assert_equal(3.1, jDouble.valueOf('3.1'))
    jFloat = Rjb::import('java.lang.Float')
    assert_equal(4.5, jFloat.valueOf('4.5'))
    jBoolean = Rjb::import('java.lang.Boolean')
    assert(jBoolean.TRUE)
    jByte = Rjb::import('java.lang.Byte')
    assert_equal(5, jByte.valueOf('5'))
    assert_equal(-6, jByte.valueOf('-6'))
  end
end

