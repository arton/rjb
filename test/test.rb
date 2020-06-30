#!/usr/local/env ruby -Ku
# encoding: utf-8
# $Id: test.rb 199 2012-12-17 13:31:18Z arton $

begin
  require 'rjb'
rescue LoadError
  require 'rubygems'
  require 'rjb'
end
require 'test/unit'
require 'fileutils'

FileUtils.rm_f 'jp/co/infoseek/hp/arton/rjb/Base.class'
FileUtils.rm_f 'jp/co/infoseek/hp/arton/rjb/ExtBase.class'

puts "start RJB(#{Rjb::VERSION}) test"
class TestRjb < Test::Unit::TestCase
  include Rjb
  def setup
    Rjb::load('.')
    Rjb::add_jar(File.expand_path('rjbtest.jar'))
    Rjb::primitive_conversion = false

    @jString = import('java.lang.String')
    @jInteger = import('java.lang.Integer')
    @jShort = import('java.lang.Short')
    @jDouble = import('java.lang.Double')
    @jFloat = import('java.lang.Float')
    @jBoolean = import('java.lang.Boolean')
    @jByte = import('java.lang.Byte')
    @jLong = import('java.lang.Long')
    @jChar = import('java.lang.Character')
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
      s = @jString.new_with_sig('Ljava.lang.String;', euc_kj)
      assert_equal(s.toString().encoding, Encoding::UTF_8)
      assert(test.isSameString(s))
      assert(test.isSameString(euc_kj))
      assert_equal(s.toString().encode(Encoding::EUC_JP), euc_kj)
      sjis_kj = "\x8a\xbf\x8e\x9a\x83\x65\x83\x4c\x83\x58\x83\x67".force_encoding Encoding::SHIFT_JIS
      s = @jString.new_with_sig('Ljava.lang.String;', sjis_kj)
      assert_equal(s.toString().encoding, Encoding::UTF_8)
      assert(test.isSameString(s))
      assert(test.isSameString(sjis_kj))
      assert_equal(s.toString().encode(Encoding::SHIFT_JIS), sjis_kj)
      utf8_kj = "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88".force_encoding Encoding::UTF_8
      s = @jString.new_with_sig('Ljava.lang.String;', utf8_kj)
      assert_equal(s.toString().encoding, Encoding::UTF_8)
      assert(test.isSameString(s))
      assert(test.isSameString(utf8_kj))
      assert_equal(s.toString().encode(Encoding::UTF_8), utf8_kj)
      iso2022jp_kj = "\x1b\x24\x42\x34\x41\x3b\x7a\x25\x46\x25\x2d\x25\x39\x25\x48\x1b\x28\x42".force_encoding Encoding::ISO_2022_JP
      s = @jString.new_with_sig('Ljava.lang.String;', iso2022jp_kj)
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
    teststr = "\xc7\x96\xc3\xbc\xcc\x84\x75\xcc\x88\xcc\x84𪚲\xe3\x81\x8b\xe3\x82\x9a"
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    s = test.getUmlaut()
    if Object::const_defined?(:Encoding) #>=1.9
=begin
      n = [teststr.bytes.length, s.bytes.length].max
      puts "org:#{teststr.bytes.length}, ret:#{s.bytes.length}"
      0.upto(n - 1) do |i|
        b0 = teststr.getbyte(i)
        b0 = 0 unless b0
        b1 = s.getbyte(i)
        b1 = 0 unless b1
        puts sprintf("%02X - %02X\n", b0, b1)
      end
=end
      assert_equal(teststr.bytes.length, s.bytes.length)
      assert_equal(teststr, s)
    else
      default_kcode = $KCODE
      begin
	$KCODE = "utf8"
	assert_equal(teststr, s)
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
    assert_equal("43210", a.concat(it))
  end

  def test_unbind()
    it = TestIter.new
    it = bind(it, 'java.util.Iterator')
    assert_equal(it, unbind(it))
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
    assert_equal(0, a.check(cp, 123, 123))
    assert_equal(5, a.check(cp, 81, 76))
    assert_equal(-5, a.check(cp, 76, 81))
  end

  # assert_raise is useless in this test, because NumberFormatException may be defined in
  # its block.
  def test_exception()
    begin
      @jInteger.parseInt('blabla')
      flunk('no exception')
    rescue NumberFormatException => e
      assert_nil(e.cause)
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
    # check class that was loaded from classpath
    loader = Rjb::import('java.lang.ClassLoader')
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

  def test_CallByNullForArrays()
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    assert_equal(nil, test.callWithArrays(nil, nil, nil, nil, nil, nil,
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

    ctest = import('jp.co.infoseek.hp.arton.rjb.Test')
    test = ctest.new
    map = test.sorted_map
    assert_equal "\0\x1\x2\x3\x4", map.get('abc')
    assert_equal "\x5\x6\x7\x8\x9", map.get('def')

    cmap = import('java.util.TreeMap')
    map = cmap.new
    map.put('abc', @jString.new('abc').bytes)
    map.put('012', @jString.new('012').bytes)

    Rjb::primitive_conversion = true
    map2 = test.throughSortedMap(map)
    assert_equal '012', map2.get('012')
    assert_equal 'abc', map2.get('abc')
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

  module TestMixin
    def test_hello(s)
      'hello ' + s
    end
  end
  def test_extend
    @jString.class_eval do
      include TestRjb::TestMixin
    end
    s = @jString.new
    assert_equal('hello world', s.test_hello('world'))
  end
  def test_extend_with_factory
    point = import('java.awt.Point')
    point.class_eval do
      include TestRjb::TestMixin
    end
    p = point.new(11, 12)
    assert_equal(11, p.x)
    assert_equal(12, p.y)
    assert_equal('hello world', p.test_hello('world'))
    p = p.location
    assert_equal(11, p.x)
    assert_equal(12, p.y)
    assert_equal('hello world', p.test_hello('world'))
  end
  def test_fetch_method_signature
    expected = ['I', 'II', 'Ljava.lang.String;', 'Ljava.lang.String;I']
    sig = @jString.sigs('indexOf').sort
    assert_equal(expected, sig)
  end
  def test_fetch_method_without_signature
    sig =
    assert_equal([nil], @jString.sigs('toString'))
  end
  def test_fetch_static_method_signature
    expected = ['Ljava.lang.String;[Ljava.lang.Object;',
                'Ljava.util.Locale;Ljava.lang.String;[Ljava.lang.Object;']
    sig = @jString.static_sigs('format').sort
    assert_equal(expected, sig)
  end
  def test_fetch_ctor_signature
    expected = ['I', 'Ljava.lang.String;']
    sig = @jInteger.ctor_sigs.sort
    assert_equal(expected, sig)
  end
  def test_methods_extension
    m = @jString.new('').methods
    assert m.include?(:indexOf)
  end
  def test_class_methods_extension
    m = @jString.methods
    assert m.include?(:format)
  end
  def test_pmethods_extension
    m = @jString.new('').public_methods
    assert m.include?(:indexOf)
  end
  def test_class_pmethods_extension
    m = @jString.public_methods
    assert m.include?(:format)
  end
  def test_java_methods
    indexof = @jString.new('').java_methods.find do |m|
      m =~ /^indexOf/
    end
    args = indexof.match(/\[([^\]]+)\]/)[1]
    assert_equal('Ljava.lang.String;I, II, I, Ljava.lang.String;'.split(/,\s*/).sort,
                 args.split(/,\s*/).sort)
  end
  def test_java_class_methods
    format = @jString.java_methods.find do |m|
      m =~ /^format/
    end
    args = format.match(/\[([^\]]+)\]/)[1]
    assert_equal('Ljava.lang.String;[Ljava.lang.Object;, Ljava.util.Locale;Ljava.lang.String;[Ljava.lang.Object;'.split(/,\s*/).sort, args.split(/,\s*/).sort)
  end
  def test_64fixnum
    big = @jLong.new_with_sig('J', 1230918239495)
    assert_equal 1230918239495, big.long_value
  end
  def test_add_jar
    add_jar(File.expand_path('./jartest.jar'))
    jt = import('jp.co.infoseek.hp.arton.rjb.JarTest')
    assert jt
    assert_equal 'abcd', jt.new.add('ab', 'cd')
  end
  def test_add_jars
    arg = ['./jartest.jar', './jartest.jar'].map do |e|
      File.expand_path(e)
    end
    add_jar(arg)
    jt = import('jp.co.infoseek.hp.arton.rjb.JarTest')
    assert_equal 'abcd', jt.new.add('ab', 'cd')
  end
  def test_bothdirection_buffer
    org = "abcdefghijklmn"
    baip = import('java.io.ByteArrayInputStream')
    ba = baip.new(org)
    buff = "\0" * org.size
    assert_equal org.size, ba.read(buff)
    assert_equal -1, ba.read(buff)
    ba.close
    assert_equal org, buff
  end
  def test_anoninterface
    arrays = import('java.util.Arrays')
    a = [3, -4, 5, -6, 8, -10, -14]
    index = arrays.binary_search(a, 6) do |m, o1, o2|
      o1.abs - o2.abs
    end
    assert_equal 3, index
    index = arrays.binary_search(a, 7) do |m, o1, o2|
      o1.abs - o2.abs
    end
    assert_equal -5, index
  end
  def test_impl
    two = import('jp.co.infoseek.hp.arton.rjb.Two')
    t = two.impl { |m| m.to_s }
    a = import('jp.co.infoseek.hp.arton.rjb.TwoCaller').new
    ret = a.foo(t)
    assert_equal 'method1', ret[0]
    assert_equal 'method2', ret[1]
  end

  def cause_exception
    begin
      @jInteger.parseInt('blabla')
    rescue NumberFormatException => e
      raise
    end
  end

  def test_reraise_exception()
    skip('1.8 does not support reraise') if /^1\.8/ =~ RUBY_VERSION
    begin
      cause_exception
    rescue
      assert($!.inspect =~ /NumberFormatException/)
    end
  end


  def test_inner_exception
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    begin
      test.cause_exception
      flunk("no exception")
    rescue IllegalStateException => e
      ia = e.cause
      assert_equal('bad argument', ia.message)
      assert_equal('java.lang.IllegalArgumentException', ia._classname)
    end
  end

  class CbTest
    def method(l, s, i, d, str)
      "test_ok:#{l}-#{s}-#{i}-#{d}-#{str}"
    end
  end
  def test_longcallback()
    cb = bind(CbTest.new, 'jp.co.infoseek.hp.arton.rjb.CallbackTest$Callback')
    test = import('jp.co.infoseek.hp.arton.rjb.CallbackTest')
    assert_equal 'test_ok:1234-1234-1234-1234.5-1234', test.callCallback(cb)
  end

  class TestIterEx < TestIter
    def initialize()
      super
      @strattr = 'strattr'
      @numattr = 32
    end
    attr_accessor :strattr, :numattr
    def multargs(a, b)
      a + b
    end
  end
  def test_method_otherthan_bound()
    it = TestIterEx.new
    it = bind(it, 'java.util.Iterator')
    test = import('jp.co.infoseek.hp.arton.rjb.Test')
    a = test.new
    assert_equal("43210", a.concat(it))
    assert(it.respond_to?(:numattr))
    assert(it.respond_to?(:multargs))
    assert_equal(32, it.numattr)
    assert_equal('strattr', it.strattr)
    it.numattr += 1
    assert_equal(33, it.numattr)
    assert_equal(5, it.multargs(3, 2))
  end
  def test_noarg_invoke()
    str = @jString.new('abc')
    assert_equal('abc', str._invoke('toString', ''))
    assert_equal('abc', str._invoke('toString', nil))
    assert_equal('abc', str._invoke('toString'))
  end
  def test_noarg_sinvoke()
    loader= import('java.lang.ClassLoader')
    sloader = loader.system_class_loader
    assert_equal(sloader._classname, loader._invoke('getSystemClassLoader', '')._classname)
    assert_equal(sloader._classname, loader._invoke('getSystemClassLoader', nil)._classname)
    assert_equal(sloader._classname, loader._invoke('getSystemClassLoader')._classname)
  end
  def test_longarg
    skip('rbx can handle 64bits long') if RUBY_ENGINE == 'rbx'
    assert_equal(597899502607411822, @jLong.reverse(0x7654321076543210))
    begin
      @jLong.reverse(0x76543210765432101)
      fail 'no exception for bigbnum it doesn\'t convert Java long'
    rescue RangeError
      assert true
    end
  end
  def test_bytearg
    b = @jByte.new(32)
    assert_equal(32, b.int_value)
    assert b.compareTo(@jByte.new(32))
    assert b.compareTo(@jByte.value_of(32))
    b = @jByte.new_with_sig('B', 32)
    assert_equal(32, b.int_value)
    assert b.compareTo(@jByte._invoke(:valueOf, 'B', 32))
  end
  def test_typedarray
    test = import('jp.co.infoseek.hp.arton.rjb.Test').new
    uri = import('java.net.URI')
    ret = test.java_typed_array(['a', 'b', 'c'], [1, 2, 3], [uri.new('http://www.artonx.org')])
    assert_equal '[Ljava.lang.String;', ret[0]
    assert_equal '[Ljava.lang.Integer;', ret[1]
    assert_equal '[Ljava.net.URI;', ret[2]
  end

  SJIS_STR = "\x8a\xbf\x8e\x9a\x83\x65\x83\x4c\x83\x58\x83\x67"
  EUCJP_STR = "\xb4\xc1\xbb\xfa\xa5\xc6\xa5\xad\xa5\xb9\xa5\xc8"
  UTF8_STR = "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x83\x86\xE3\x82\xAD\xE3\x82\xB9\xE3\x83\x88"
  def test_auto_constructor_selection
    skip 'no encoding' unless Object::const_defined?(:Encoding)
    sys = import('java.lang.System')
    encoding = sys.property('file.encoding')
    s = @jString.new(SJIS_STR.force_encoding Encoding::SHIFT_JIS)
    e = @jString.new(EUCJP_STR.force_encoding Encoding::EUC_JP)
    u = @jString.new(UTF8_STR.force_encoding Encoding::UTF_8)
    if encoding == 'MS932'
      s1 = @jString.new(SJIS_STR.bytes)
    elsif encoding.upcase == 'EUC-JP'
      s1 = @jString.new(EUCJP_STR.bytes)
    elsif encoding.upcase == 'UTF-8'
      s1 = @jString.new(UTF8_STR.bytes)
    else
      skip 'no checkable encoding'
    end
    assert_equal s1.toString, s.toString
    assert_equal s1.toString, e.toString
    assert_equal s1.toString, u.toString
  end

  def test_bothdirection_chararray
    charArrayReader = import('java.io.CharArrayReader')
    org = [48, 49, 50, 51, 52, 53]
    reader = charArrayReader.new(org)
    buffer = Array.new(32, 0)
    len = reader.read(buffer, 0, buffer.size)
    assert_equal org.size, len
    assert_equal org, buffer[0...len]
  end

  def test_re_raise
    begin
      @jInteger.parseInt('blabla')
      flunk('no exception')
    rescue NumberFormatException => e
      begin
        raise
      rescue => e
        assert_equal(NumberFormatException, e.class)
        # OK
      end
    end
  end

  def test_java_utf8
    y = @jString.new('𠮷野家')
    assert_equal '𠮷野家', y.toString
  end

  def test_respond_to
    str = @jString.new('blabla')
    assert str.respond_to? :substring
    assert_false str.respond_to? :unknown_method
    begin
      @jInteger.parseInt('blabla')
    rescue => e
      assert e.respond_to? :print_stack_trace
      assert e.respond_to? :printStackTrace
      assert_false e.respond_to? :unknown_method
    end
  end
end
