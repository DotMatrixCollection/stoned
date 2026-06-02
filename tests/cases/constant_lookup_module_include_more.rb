module ConfigDefaults
  DEFAULT_NAME = "default"

  def default_name
    DEFAULT_NAME
  end
end

class ConfigHost
  include ConfigDefaults
  DEFAULT_NAME = "host"

  class Nested
    def self.host_default
      ConfigHost::DEFAULT_NAME
    end
  end

  def own_name
    DEFAULT_NAME
  end
end

host = ConfigHost.new
p host.default_name
p host.own_name
p ConfigHost::Nested.host_default
p ConfigDefaults.const_defined?(:DEFAULT_NAME)
