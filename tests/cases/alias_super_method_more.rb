class AliasOverrideSample
  def value
    "first"
  end

  alias old_value value

  def value
    "second"
  end
end

obj = AliasOverrideSample.new
p obj.value
p obj.old_value
p obj.method(:old_value).call
