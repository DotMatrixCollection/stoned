module StateMachine
  def self.included(base)
    base.extend(ClassMethods)
    base.instance_variable_set(:@states, [])
    base.instance_variable_set(:@transitions, {})
    base.instance_variable_set(:@initial_state, nil)
  end

  module ClassMethods
    def state(name, initial: false)
      @states << name
      @initial_state = name if initial
    end

    def event(name, transitions:)
      transitions.each do |from, to|
        @transitions[from] ||= {}
        @transitions[from][name] = to
      end
      define_method("#{name}!") do
        current = @state
        next_state = self.class.instance_variable_get(:@transitions).dig(current, name)
        raise "Cannot #{name} from #{current}" unless next_state
        @state = next_state
      end
    end
  end

  def initialize
    @state = self.class.instance_variable_get(:@initial_state)
  end

  def state
    @state
  end
end

class Order
  include StateMachine
  state :pending, initial: true
  state :confirmed
  event :confirm, transitions: { pending: :confirmed }
end

o = Order.new
puts o.state
o.confirm!
puts o.state

begin
  o.confirm!
rescue RuntimeError => e
  puts e.message
end
