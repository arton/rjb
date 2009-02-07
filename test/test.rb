#!/usr/local/env ruby
# $Id$

begin
  require 'rjb'
rescue LoadError 
  require 'rubygems' 
  require 'rjb'
end
require 'test/unit'

puts "start RJB(#{Rjb::VERSION}) test"
class TestRjb < Test::Unit::TestCase
  include Rjb
  def setup
    load('.')
    Rjb::primitive_conversion = false

    @jString = import('java.lang.String')
    @jInteger = Rjb::import('java.lang.Integer')
    @jShort = Rjb::import('java.lang.Short')
    @jDouble = Rjb::import('java.lang.Double')
    @jFloat = Rjb::import('java.lang.Float')
    @jBoolean = Rjb::import('java.lang.Boolean')
    @jByte = Rjb::import('java.lang.Byte')
    @jLong = Rjb::import('java.lang.Long')
    @jChar = Rjb::import('java.lang.Character')
  end

  def teardown
  end

  def test_metaclass
    cls = import('java.lang.Class')
    assert_equal('java.lang.Class', cls._classname)
    assert_equal('java.lang.Class', cls.getName)
    assert_equal(17, cls.getModifiers)
  end

  def test_scalar
    assert_equal('java.lang.Class', @jString._classname)
    assert_equal('class java.lang.String', @jString.toString)
    str = @jString.new
    assert_equal('java.lang.String', str._classname)
    assert_equal(0, str.length)
    assert_equal('', str.toString)
    str = @jString.new_with_sig('Ljava.lang.String;', "abcde")
    # result integer
    assert_equal(5, str.length)
    # result string
    assert_equal('abcde', str.toString)
    # argument test
    # char
    assert_equal('abxde', str.replace("c".sum, "x".sum))
    # string
    assert_equal('abdxe', str.replaceAll('cd', 'dx'))
    # int
    assert_equal('bc', str.substring(1, 3))
    assert_equal('e', str.substring(4))
    # float with static
    assert_equal('5.23', @jString._invoke('valueOf', 'F', 5.23))
    assert_equal('25.233', @jString._invoke('valueOf', 'D', 25.233))
    # rjb object (String)
    str2 = @jString.new_with_sig('Ljava.lang.String;', 'fghijk')
    assert_equal('abcdefghijk', str.concat(str2))
    # rjb object other (implicit toString call is Rjb feature)
    i = @jInteger.new_with_sig('I', 35901)
    assert_equal('abcde35901', str.concat(i))
    # result boolean and argument is rjb object
    assert_equal(false, i.equals(str))
    assert_equal(false, str.equals(i))
    assert_equal(true, str.equals("abcde"))
    assert_equal(true, str.equals(str))
    # long
    l = @jLong.new_with_sig('J', -9223372036854775808)
    assert_equal(-9223372036854775808, l.longValue)
    l = @jLong.new_with_sig('J', 9223372036854775807)
    assert_equal(9223372036854775807, l.longValue)
    # double
    d = @jDouble.new_with_sig('D', 1234.5678901234567890)
    assert_equal(1234.5678901234567890, d.doubleValue)
    # byte
    b = @jByte.new_with_sig('B', 13)
    assert_equal(13, b.byteValue)
    # float
    f = @jFloat.new_with_sig('F', 13.5)
    assert_equal(13.5, f.floatValue)
    # short
    s = @jShort.new_with_sig('S', 1532)
    assert_equal(1532, s.shortValue)
    c = @jChar.new_with_sig('C', "A".sum)
    assert_equal("A".sum, c.charValue)
  end

  def test_array
    str = @jString.new_with_sig('[C', ["a".sum, "b".sum, "c".sum, "d".sum, "e".sum, "c".sum, "f".sum, "c".sum, "g".sum])
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
    assert_equal(["a".sum, "b".sum, "c".sum, "d".sum, "e".sum, "c".sum, "f".sum, "c".sum, "g".sum], ca)
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
    assert_equal(@jBoolean.valueOf(true).booleanValue(), true)
    assert_equal(@jBoolean.valueOf(false).booleanValue(), false)
    assert_equal(@jBoolean.valueOf('true').booleanValue(), true)
    assert_equal(@jBoolean.valueOf('false').booleanValue(), false)
  end

  def test_importobjarray()
    jarray = import('java.util.ArrayList')
    a = jarray.new()
    a.add(@jInteger.new_with_sig('I', 1))
    a.add(@jInteger.new_with_sig('I', 2))
    a.add(@jInteger.new_with_sig('I', 3))
    oa = a.toArray
    assert_equal(3, oa.size)
    assert_equal(1, oa[0].intValue)
    assert_equal(2, oa[1].intValue)    
    assert_equal(3, oa[2].intValue)    
  end

  def test_kjconv()
    if Object::const_defined?(:Encoding)
      test = import('jp.co.infoseek.hp.arton.rjb.Test').new
      euc_kj = "\xb4\xc1\xbb\xfa\xa5\xc6\xa5\xad\xa5\xb9\xa5\xc8".force_encoding Encoding::EUC_JP
      s = @jString.new(euc_kj)
      assert_equal(s.toString().encoding, Encoding::UTF_8)
      assert(test.isSameString(s))
      assert(test.isSameString(euc_kj))
      assert_equal(s.toString().encode(Encoding::EUC_JP), euc_kj)
      sjis_kj = "\x8a\xbf\x8e\x9a\x83\x65\x83\x4c\x83\x58\x83\x67".force_encoding Encoding::SHIFT_JIS
      s = @jString.new(sjis_kj)
      assert_equal(s.toString().encoding, Encoding::UTF_8)
      assert(test.isSameString(s))
      assert(test.isSameString(sjis_kj))
      assert_equal(s.toString().encode(Encoding::SHIFT_JIS), sjis_kj)
      utf8_kj = "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88".force_encoding Encoding::UTF_8
      s = @jString.new(utf8_kj)
      assert_equal(s.toString().encoding, Encoding::UTF_8)
      assert(test.isSameString(s))
      assert(test.isSameString(utf8_kj))
      assert_equal(s.toString().encode(Encoding::UTF_8), utf8_kj)
      iso2022jp_kj = "\x1b\x24\x42\x34\x41\x3b\x7a\x25\x46\x25\x2d\x25\x39\x25\x48\x1b\x28\x42".force_encoding Encoding::ISO_2022_JP
      s = @jString.new(iso2022jp_kj)
      assert_equal(s.toString().encoding, Encoding::UTF_8)
      assert(test.isSameString(s))
      assert(test.isSameString(iso2022jp_kj))
      assert_equal(s.toString().encode(Encoding::ISO_2022_JP), iso2022jp_kj)
      assert_equal(@jString.new("abcdef".force_encoding(Encoding::ASCII_8BIT)).toString(), "abcdef")
      assert_equal(@jString.new("abcdef".force_encoding(Encoding::find("us-ascii"))).toString(), "abcdef")
    else
      default_kcode = $KCODE
      begin
	$KCODE = 'euc'
	euc_kj = "\xb4\xc1\xbb\xfa\xa5\xc6\xa5\xad\xa5\xb9\xa5\xc8"
	s = @jString.new(euc_kj)
	assert_equal(s.toString(), euc_kj)
	$KCODE = 'sjis'
	sjis_kj = "\x8a\xbf\x8e\x9a\x83\x65\x83\x4c\x83\x58\x83\x67"
	s = @jString.new(sjis_kj)
	assert_equal(s.toString(), sjis_kj)
	$KCODE = 'utf8'
	utf8_kj = "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88"
	s = @jString.new(utf8_kj)
	assert_equal(s.toString(), utf8_kj)
	$KCODE = 'none'
	if /mswin(?!ce)|mingw|cygwin|bccwin/ =~ RUBY_PLATFORM
	  #expecting shift_jis on windows
	  none_kj = "\x8a\xbf\x8e\x9a\x83\x65\x83\x4c\x83\x58\x83\x67"
	else
	  #expecting utf-8 unless windows
	  none_kj = "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88"
	end
	s = @jString.new(none_kj)
	assert_equal(s.toString(), none_kj)
	$KCODE = 'utf8'
	utf8_kj = "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88"
	s = @jString.new(utf8_kj)
	assert_equal(s.toString(), utf8_kj)
	$KCODE = 'sjis'
	sjis_kj = "\x8a\xbf\x8e\x9a\x83\x65\x83\x4c\x83\x58\x83\x67"
	s = @jString.new(sjis_kj)
	assert_equal(s.toString(), sjis_kj)
	$KCODE = 'euc'
	euc_kj = "\xb4\xc1\xbb\xfa\xa5\xc6\xa5\xad\xa5\xb9\xa5\xc8"
	s = @jString.new(euc_kj)
	assert_equal(s.toString(), euc_kj)
      ensure
	$KCODE = default_kcode
      end
    end
  end

  def test_combination_charcters
    teststr = "\xc7\x96\xc3\xbc\xcc\x84\x75\xcc\x88\xcc\x84\xed\xa1\xa9\xed\xba\xb2\xe3\x81\x8b\xe3\x82\x9a"
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    s = test.getUmlaut()
    if Object::const_defined?(:Encoding) #>=1.9
      teststr = teststr.force_encoding(Encoding::UTF_8)
      assert_equal(s, teststr)
    else
      default_kcode = $KCODE
      begin
	$KCODE = "utf8"
	assert_equal(s, teststr)
      ensure
	$KCODE = default_kcode
      end
    end
  end

  def test_constants()
    assert_equal(0x7fffffffffffffff, @jLong.MAX_VALUE)
    assert_equal(-9223372036854775808, @jLong.MIN_VALUE)
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

  def test_unbind()
    it = TestIter.new
    it = bind(it, 'java.util.Iterator')
    assert(it, unbind(it))
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
    begin
      @jInteger.parseInt('blabla')
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
      throw(@jString.new('a'))
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

  def test_throw_clear()
    assert_nothing_raised {
      begin
	Rjb::throw('java.util.NoSuchElementException', 'test exception')
      rescue #drop ruby exception
      end
      test = import('jp.co.infoseek.hp.arton.rjb.Test')
      begin
	Rjb::throw('java.util.NoSuchElementException', 'test exception')
      rescue #drop ruby exception
      end
      test.new
      begin
	Rjb::throw('java.util.NoSuchElementException', 'test exception')
      rescue #drop ruby exception
      end
      @jString.new_with_sig('Ljava.lang.String;', "abcde")
      begin
	Rjb::throw('java.util.NoSuchElementException', 'test exception')
      rescue #drop ruby exception
      end
      it = TestIterator.new(0)
      it = bind(it, 'java.util.Iterator')
      begin
	Rjb::throw('java.util.NoSuchElementException', 'test exception')
      rescue NoSuchElementException
      end
      begin
	Rjb::throw('java.lang.IllegalAccessException', 'test exception')
      rescue IllegalAccessException
      end
      unbind(it)
    }
  end

  def test_field()
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    assert_equal('Hello World !!', test.helloData)
    test.helloData = 'Goodby World !'
    assert_equal('Goodby World !', test.helloData)
  end

  def test_instancemethod_from_class()
    begin
      assert_equal('true', @jString.valueOf(true))
      @jString.length
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
    begin
      s = @jString.new('a', 'b', 'c')
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
    sb.set_char_at(1, "B".sum)
    assert_equal('aBc', sb.to_string)
    sb.length = 2
    assert_equal('aB', sb.to_string)
  end

  def test_auto_conv
    assert_equal(false, Rjb::primitive_conversion)
    Rjb::primitive_conversion = true
    assert_equal(true, Rjb::primitive_conversion)
    assert_equal(1, @jInteger.valueOf('1'))
    assert_equal(-1, @jInteger.valueOf('-1'))
    assert_equal(2, @jShort.valueOf('2'))
    assert_equal(-2, @jShort.valueOf('-2'))
    assert_equal(3.1, @jDouble.valueOf('3.1'))
    assert_equal(4.5, @jFloat.valueOf('4.5'))
    assert(@jBoolean.TRUE)
    assert_equal(5, @jByte.valueOf('5'))
    assert_equal(-6, @jByte.valueOf('-6'))
    assert_equal(0x7000000000000000, @jLong.valueOf('8070450532247928832'))
    assert_equal(-9223372036854775807, @jLong.valueOf('-9223372036854775807'))
    assert_equal("A".sum, @jChar.valueOf("A".sum))
  end

  def test_obj_to_primitive
    ar = Rjb::import('java.util.ArrayList')
    a = ar.new
    a.add @jString.new('abcdef')
    a.add @jInteger.valueOf('1')
    a.add @jShort.valueOf('2')
    a.add @jDouble.valueOf('3.1')
    a.add @jFloat.valueOf('4.5')
    a.add @jBoolean.TRUE
    a.add @jByte.valueOf('5')
    a.add @jLong.valueOf('8070450532247928832')
    a.add @jChar.valueOf("A".sum)

    Rjb::primitive_conversion = true

    assert_equal 'abcdef', a.get(0)
    assert_equal 1, a.get(1)
    assert_equal 2, a.get(2)
    assert_equal 3.1, a.get(3)
    assert_equal 4.5, a.get(4)
    assert a.get(5)
    assert_equal 5, a.get(6)
    assert_equal 8070450532247928832, a.get(7)
    assert_equal "A".sum, a.get(8)
  end

  def test_primitive_to_obj
    Rjb::primitive_conversion = true

    ar = Rjb::import('java.util.ArrayList')
    a = ar.new
    a.add @jString.new('abcdef')
    a.add @jInteger.valueOf('1')
    a.add @jShort.valueOf('2')
    a.add @jDouble.valueOf('3.1')
    a.add @jFloat.valueOf('4.5')
    a.add @jBoolean.TRUE
    a.add @jByte.valueOf('5')
    a.add @jLong.valueOf('8070450532247928832')
    a.add @jChar.valueOf("A".sum)
    assert_equal 'abcdef', a.get(0)
    assert_equal 1, a.get(1)
    assert_equal 2, a.get(2)
    assert_equal 3.1, a.get(3)
    assert_equal 4.5, a.get(4)
    assert a.get(5)
    assert_equal 5, a.get(6)
    assert_equal 8070450532247928832, a.get(7)
    assert_equal "A".sum, a.get(8)
  end

  def test_enum
    t = Rjb::import('jp.co.infoseek.hp.arton.rjb.Test$TestTypes')
    assert t.ONE.equals(t.values()[0])
    assert_equal 3, t.values().size
    assert_equal 2, t.THREE.ordinal
    assert_equal "TWO", t.TWO.name
    assert_equal "THREE", t.THREE.toString
  end

  #rjb-bugs-15430 rebported by Bryan Duxbury
  def test_generics_map
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    map = test.sorted_map
    assert_equal "\0\x1\x2\x3\x4", map.get('abc')
    assert_equal "\x5\x6\x7\x8\x9", map.get('def')
  end

  def x_test_zzunload
    # this test should run at the last
    unload
    begin
      load('.')
      fail 'no exception'
    rescue
      assert_equal "can't create Java VM", $!.message
    end
  end
end

